#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/CoreComponent/CELComponentBase.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedTargets.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedAnomalies.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedDeaths.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedSight.h"
#include "ai/monsters/control_direction_base.h"
#include "ai/ExtendedEntityesLogic/EntryPoint.h"
#include "ai/ExtendedEntityesLogic/CELSharedSettings.h"
#include "ai/ExtendedEntityesLogic/CELSharedPerceptionBudget.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "ai_object_location.h"
#include "memory_manager.h"
#include "enemy_manager.h"
#include "visual_memory_manager.h"
#include "Actor.h"
#include "../Debug/Debug.h"
#include "Level.h"
#include "ai_space.h"
#include "level_graph.h"
#include "../xrSound/ai_sounds.h"

// Связывает общую память и восприятие владельца с необходимыми службами и состоянием владельца.
CELComponentBase::CELComponentBase(xr_unique_ptr<IELAdapter> OwnerAdapter, const SELPerceptionProfile& OwnerProfile) : Adapter(std::move(OwnerAdapter)), Profile(OwnerProfile) {}

// Подключает владельца к службам расширения и создаёт начальное состояние.
void CELComponentBase::Attach()
{
	if (HasAttached)
	{
		return;
	}
	ReceiveAfter = 0;
	HasAttached = true;
}

// Снимает управление расширения и освобождает регистрации владельца.
void CELComponentBase::Detach()
{
	if (!HasAttached)
	{
		return;
	}
	Adapter->ReleaseControl();
	Adapter->Monster().EnemyMemory.clear();
	Memory.Clear();
	Hearing.Clear();
	Sharing.Clear();
	VisualAcquisition.clear();
	TargetIdentifier = u16(-1);
	HasAttached = false;
}

// Проверяет, истёк ли срок зрительного распознавания цели.
bool CELComponentBase::HasVisualAcquired(CEntityAlive& Candidate, u32 CurrentTime)
{
	if (!smart_cast<CActor*>(&Candidate) || !Profile.VisualAcquireTime)
	{
		return true;
	}
	SVisualAcquisition& Acquisition = VisualAcquisition[Candidate.ID()];
	if (Acquisition.SampleTime && CurrentTime - Acquisition.SampleTime > Profile.MemoryMax)
	{
		Acquisition.VisibleTime = 0;
	}
	Acquisition.VisibleTime += Acquisition.SampleTime ? std::min(100u, CurrentTime - Acquisition.SampleTime) : 0;
	Acquisition.SampleTime = CurrentTime;
	return Acquisition.VisibleTime >= Profile.VisualAcquireTime;
}

// Записывает подтверждённые сведения о сущности в личную память.
void CELComponentBase::RememberEntity(CEntityAlive& TargetEntity, EELObservationSource Source, u32 ObservedAt)
{
	if (ExtendedEntityes::IsIgnored(TargetEntity))
	{
		return;
	}
	SELObservation Observation;
	Observation.TargetIdentifier = TargetEntity.ID();
	Observation.Position = TargetEntity.Position();
	Observation.VertexIdentifier = TargetEntity.ai_location().level_vertex_id();
	Observation.ObservedAt = ObservedAt;
	Observation.Source = Source;
	if (Source == EELObservationSource::Vision && Adapter->Monster().Position().distance_to_sqr(Observation.Position) <= Profile.CloseVisualRadius * Profile.CloseVisualRadius)
	{
		Observation.VisualGraceUntil = ObservedAt + Profile.CloseVisualTime;
	}
	Memory.Remember(Observation, Device.dwTimeGlobal, Random.randI(Profile.MemoryMin, Profile.MemoryMax + 1), Profile.CanRecordTrail ? Profile.TrailLength : 0.f);
}

// Обновляет личные наблюдения владельца с учётом доступного бюджета.
void CELComponentBase::Observe()
{
	if (!HasAttached)
	{
		return;
	}
	CBaseMonster& OwnerMonster = Adapter->Monster();
	const u32 CurrentTime = Device.dwTimeGlobal;
	if (Actor() && ExtendedEntityes::IsIgnored(*Actor()))
	{
		ForgetTarget(Actor()->ID());
	}
	Hearing.PublishDebug(OwnerMonster.Position(), Profile, OwnerMonster.ID());
	ObserveVision(OwnerMonster, CurrentTime);
	ObserveDamage(OwnerMonster, CurrentTime);
	CELSharedDeaths::UpdateCombat(OwnerMonster, Profile, OwnerMonster.EnemyMan.see_enemy_now());
	CELSharedDeaths::Observe(OwnerMonster, Profile);
	CELSharedAnomalies::Observe(OwnerMonster, Profile);
	ObserveCloseContact();
	ObservePeriphery();
	Memory.Expire(CurrentTime);
	Sharing.Update(OwnerMonster, Memory, Profile);
	const SELObservation* SelectedObservation = Profile.IsPassive ? nullptr : CELSharedTargets::Select(Memory, *Adapter, TargetIdentifier);
	TargetIdentifier = SelectedObservation ? SelectedObservation->TargetIdentifier : u16(-1);
	PublishNativeMemory();
	if (SelectedObservation && CELSharedDebug::Instance().IsDrawEnabled())
	{
		SELDebugEvent Event;
		Event.Kind = EELDebugEvent::Trail;
		Event.OwnerIdentifier = OwnerMonster.ID();
		Event.TargetIdentifier = TargetIdentifier;
		Event.Start = OwnerMonster.Position();
		Event.Finish = SelectedObservation->Position;
		Event.Points = SelectedObservation->Trail;
		Event.Radius = .25f;
		Event.Label = "Последняя известная точка / след";
		CELSharedDebug::Instance().PublishEvent(std::move(Event));
	}
}

// Передаёт актуальные подтверждённые знания в штатную память монстра.
void CELComponentBase::PublishNativeMemory()
{
	CBaseMonster& OwnerMonster = Adapter->Monster();
	OwnerMonster.EnemyMemory.clear();
	if (Profile.IsPassive)
	{
		return;
	}
	CELSharedMemory::Records::const_iterator ObservationIterator;
	CEntityAlive* TargetEntity;
	Fvector KnownPosition;
	u32 KnownVertex;
	for (ObservationIterator = Memory.GetRecords().begin(); ObservationIterator != Memory.GetRecords().end(); ++ObservationIterator)
	{
		TargetEntity = ExtendedEntityes::FindEntity(ObservationIterator->TargetIdentifier);
		if (!TargetEntity || !Adapter->IsEnemy(*TargetEntity))
		{
			continue;
		}
		KnownPosition = ObservationIterator->Position;
		KnownVertex = ObservationIterator->VertexIdentifier;
		if (Profile.CanTrackLive || s32(Device.dwTimeGlobal - ObservationIterator->VisualGraceUntil) < 0)
		{
			KnownPosition = TargetEntity->Position();
			KnownVertex = TargetEntity->ai_location().level_vertex_id();
		}
		OwnerMonster.EnemyMemory.add_enemy(TargetEntity, KnownPosition, KnownVertex, ObservationIterator->ObservedAt);
	}
}

// Принимает сведения другого источника с проверкой правил передачи.
bool CELComponentBase::Accept(const SELObservation& Observation, CBaseMonster& SourceMonster)
{
	CBaseMonster& OwnerMonster = Adapter->Monster();
	if (!HasAttached || Profile.IsPassive || (!Observation.HasParentalChannel && s32(Device.dwTimeGlobal - ReceiveAfter) < 0 && (LastSender != SourceMonster.ID() || LastReceivedAt != Device.dwTimeGlobal)) || !CELSharedEnemySharing::CanReceive(OwnerMonster, SourceMonster))
	{
		return false;
	}
	const SELPerceptionProfile& SourceProfile = CELSharedSettings::Profile(*SourceMonster.cNameSect());
	CEntityAlive* TargetEntity = ExtendedEntityes::FindEntity(Observation.TargetIdentifier);
	if (Observation.Kind == EELObservationKind::Enemy && (!TargetEntity || !Adapter->IsEnemy(*TargetEntity)))
	{
		return false;
	}
	const float Distance = OwnerMonster.Position().distance_to(SourceMonster.Position());
	if (!Observation.HasParentalChannel && (Distance > SourceProfile.SharingRadius || (!SourceProfile.IsWireless && Distance > SourceProfile.SharingRadius * (Observation.HasCall ? SourceProfile.CallSharingRatio : SourceProfile.CloseSharingRatio) && !Adapter->CanSee(SourceMonster))))
	{
		return false;
	}
	if (Observation.Kind != EELObservationKind::Enemy)
	{
		if (!ExtendedEntityes::PerceptionBudget().TryTransfer() || !CELSharedDeaths::Remember(OwnerMonster, Observation, Profile))
		{
			return false;
		}
		ReceiveAfter = Device.dwTimeGlobal + Random.randI(Profile.SharingIntervalMin, Profile.SharingIntervalMax + 1);
		LastReceivedAt = Device.dwTimeGlobal;
		LastSender = SourceMonster.ID();
		return true;
	}
	const SELObservation* ExistingObservation = Memory.Find(Observation.TargetIdentifier);
	if (ExistingObservation && (s32(Observation.ObservedAt - ExistingObservation->ObservedAt) < 0 || (!Observation.HasLivePosition && Observation.ObservedAt - ExistingObservation->ObservedAt < 1000 && Observation.Position.distance_to_sqr(ExistingObservation->Position) < 16.f)))
	{
		return false;
	}
	if (!ExtendedEntityes::PerceptionBudget().TryTransfer() || !Memory.Remember(Observation, Device.dwTimeGlobal, Random.randI(Profile.MemoryMin, Profile.MemoryMax + 1), Profile.CanRecordTrail ? Profile.TrailLength : 0.f))
	{
		return false;
	}
	ReceiveAfter = Device.dwTimeGlobal + Random.randI(Profile.SharingIntervalMin, Profile.SharingIntervalMax + 1);
	LastReceivedAt = Device.dwTimeGlobal;
	LastSender = SourceMonster.ID();
	if (Observation.HasCall)
	{
		Sharing.OnReceivedCall(Profile.CallCooldown);
	}
	if (Observation.HasParentalChannel)
	{
		TargetIdentifier = Observation.TargetIdentifier;
		PublishNativeMemory();
		OwnerMonster.EnemyMan.update();
	}
	return true;
}

// Обрабатывает принятое звуковое событие по настроенным правилам реакции.
void CELComponentBase::Hear(const SELSoundEvent& SoundEvent)
{
	if (!HasAttached || SoundEvent.SourceIdentifier == Adapter->Monster().ID())
	{
		return;
	}
	CEntityAlive* SourceEntity = ExtendedEntityes::FindEntity(SoundEvent.SourceIdentifier);
	const bool IsDeath = CELSharedHearing::Classify(SoundEvent) == EELNoiseType::Death;
	const bool WasHeard = Hearing.Hear(SoundEvent, Adapter->Monster().Position(), Profile, Adapter->Monster().ID());

	CBaseMonster* SourceMonster = SourceEntity ? smart_cast<CBaseMonster*>(SourceEntity) : nullptr;
	if (WasHeard && IsDeath && Profile.CanHearDeaths && (Profile.InterestNoiseTypes & ELNoiseTypeMask(EELNoiseType::Death)) &&
		SourceMonster && !SourceMonster->g_Alive() && CELSharedEnemySharing::AreFriends(Adapter->Monster(), *SourceMonster) &&
		Adapter->Monster().Position().distance_to_sqr(SoundEvent.Position) <= Profile.DeathHearingRadius * Profile.DeathHearingRadius)
	{
		Hearing.SetSource(SoundEvent.SourceIdentifier);
		Hearing.AttentionPosition = SoundEvent.Position;
		Hearing.InvestigateUntil = SoundEvent.OccurredAt + Profile.SearchMax;
	}
}

// Удаляет знания и ссылки, связанные с исчезнувшим объектом.
void CELComponentBase::RemoveObject(u16 ObjectIdentifier)
{
	ForgetTarget(ObjectIdentifier);
	Hearing.RemoveObject(ObjectIdentifier);
}

// Удаляет сведения о противнике, не затрагивая слуховое внимание к его звукам.
void CELComponentBase::ForgetTarget(u16 ObjectIdentifier)
{
	const bool HadObservation = Memory.Find(ObjectIdentifier) != nullptr;
	Memory.Remove(ObjectIdentifier);
	VisualAcquisition.erase(ObjectIdentifier);
	if (TargetIdentifier == ObjectIdentifier)
	{
		TargetIdentifier = u16(-1);
		CELSharedTargets::RemoveOwner(Adapter->Monster().ID());
	}
	if (HadObservation && HasAttached)
	{
		PublishNativeMemory();
	}
}

// Выполняет покадровую часть активной схемы без повторного выбора поведения.
void CELComponentBase::FrameUpdate(float DeltaTime) {}

// Проверяет близкие угрозы, доступные контактному обнаружению.
void CELComponentBase::ObserveCloseContact()
{
	if (Profile.IsPassive || Profile.CloseDetectionRadius <= 0.f || s32(Device.dwTimeGlobal - NextCloseProbe) < 0)
	{
		return;
	}
	NextCloseProbe = Device.dwTimeGlobal + Random.randI(200u, 501u);
	if (!ExtendedEntityes::PerceptionBudget().TrySpatial())
	{
		return;
	}
	CBaseMonster& OwnerMonster = Adapter->Monster();
	xr_vector<ISpatialShared> Candidates;
	g_SpatialSpace->q_sphere(Candidates, 0, ESPATIAL_TYPE::COLLIDEABLE, OwnerMonster.Position(), Profile.CloseDetectionRadius);
	xr_vector<ISpatialShared>::const_iterator CandidateIterator;
	CObject* CandidateObject;
	CEntityAlive* Candidate;
	for (CandidateIterator = Candidates.begin(); CandidateIterator != Candidates.end(); ++CandidateIterator)
	{
		CandidateObject = *CandidateIterator ? (*CandidateIterator)->dcast_CObject() : nullptr;
		Candidate = CandidateObject ? CandidateObject->cast_entity_alive() : nullptr;
		if (!Candidate || !Adapter->IsEnemy(*Candidate))
		{
			continue;
		}
		if (CELSharedSight::Visible(OwnerMonster, *Candidate, 360.f, Profile.CloseDetectionRadius))
		{
			RememberEntity(*Candidate, EELObservationSource::CloseContact, Device.dwTimeGlobal);
		}
	}
}

// Пополняет личную память целями, подтверждёнными зрением.
void CELComponentBase::ObserveVision(CBaseMonster& OwnerMonster, u32 CurrentTime)
{
	const CObjectManager<const CEntityAlive>::OBJECTS& Candidates = OwnerMonster.memory().enemy().objects();
	CObjectManager<const CEntityAlive>::OBJECTS::const_iterator CandidateIterator;
	CEntityAlive* Candidate;
	for (CandidateIterator = Candidates.begin(); CandidateIterator != Candidates.end(); ++CandidateIterator)
	{
		Candidate = const_cast<CEntityAlive*>(*CandidateIterator);
		if (Adapter->IsEnemy(*Candidate) && Adapter->CanSee(*Candidate) && HasVisualAcquired(*Candidate, CurrentTime))
		{
			RememberEntity(*Candidate, EELObservationSource::Vision, CurrentTime);
		}
	}
}

// Запоминает противника, установленного по полученному урону.
void CELComponentBase::ObserveDamage(CBaseMonster& OwnerMonster, u32 CurrentTime)
{
	CObject* Attacker = OwnerMonster.HitMemory.get_last_hit_object();
	CEntityAlive* Candidate = Attacker ? Attacker->cast_entity_alive() : nullptr;
	if (Candidate && Adapter->IsEnemy(*Candidate) && CurrentTime - OwnerMonster.HitMemory.get_last_hit_time() < 1000)
	{
		if (Profile.CanAutoFocus && OwnerMonster.Position().distance_to(Candidate->Position()) <= Profile.AutoFocusRadius)
		{
			RememberEntity(*Candidate, EELObservationSource::Damage, OwnerMonster.HitMemory.get_last_hit_time());
		}
		if (Profile.CanPanicOnDamage && !Adapter->CanSee(*Candidate))
		{
			Hearing.SetSource(Candidate->ID());
			Hearing.AttentionPosition = OwnerMonster.HitMemory.get_last_hit_position();
			Hearing.PanicUntil = OwnerMonster.HitMemory.get_last_hit_time() + 10000;
		}
	}
}

// Проверяет периферийные стимулы и направляет осмотр без всеведения.
void CELComponentBase::ObservePeriphery()
{
	CBaseMonster& OwnerMonster = Adapter->Monster();
	if (!Profile.CanUsePeriphery || Profile.IsPassive || !Adapter->CanControl() || OwnerMonster.EnemyMan.see_enemy_now() || s32(Device.dwTimeGlobal - NextPeripheralProbe) < 0)
	{
		return;
	}
	if (!ExtendedEntityes::PerceptionBudget().TrySpatial())
	{
		return;
	}
	NextPeripheralProbe = Device.dwTimeGlobal + Profile.PeripheralInterval + OwnerMonster.ID() % 201;
	xr_vector<ISpatialShared> Candidates;
	g_SpatialSpace->q_sphere(Candidates, 0, ESPATIAL_TYPE::COLLIDEABLE, OwnerMonster.Position(), Profile.PeripheralRadius);
	if (Candidates.empty())
	{
		return;
	}
	u32 Checked;
	CObject* CandidateObject;
	CEntityAlive* Candidate;
	for (Checked = 0; Checked < Candidates.size() && Checked < 64; ++Checked)
	{
		PeripheralCursor %= u32(Candidates.size());
		CandidateObject = Candidates[PeripheralCursor] ? Candidates[PeripheralCursor]->dcast_CObject() : nullptr;
		++PeripheralCursor;
		Candidate = CandidateObject ? CandidateObject->cast_entity_alive() : nullptr;
		if (!Candidate || !Adapter->IsEnemy(*Candidate))
		{
			continue;
		}
		if (CELSharedSight::Visible(OwnerMonster, *Candidate, std::max(Profile.PeripheralAngle, OwnerMonster.eye_fov), std::min(Profile.PeripheralRadius, OwnerMonster.eye_range)))
		{
			// Attention rotates the native view; only native confirmed vision may add an enemy fact.
			OwnerMonster.dir().face_target(Candidate, 800);
			break;
		}
	}
}
