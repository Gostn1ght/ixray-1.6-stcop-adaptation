#include "stdafx.h"
#include "ai/monsters/control_animation_base.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Ground/CELGround.h"
#include "ai/ExtendedEntityesLogic/Debug/Debug.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "ai_monster_space.h"

// Связывает общую логику наземной категории с необходимыми службами и состоянием владельца.
CELGround::CELGround(xr_unique_ptr<IELAdapter> OwnerAdapter, const SELPerceptionProfile& OwnerProfile) : CELComponentBase(std::move(OwnerAdapter), OwnerProfile), BehaviorManager(*this, *Adapter, Profile) {}
// Завершает общую логику наземной категории и освобождает связанные ресурсы.
CELGround::~CELGround()
{
	BehaviorManager.Clear();
}

// Снимает управление расширения и освобождает регистрации владельца.
void CELGround::Detach()
{
	BehaviorManager.Clear();
	CELComponentBase::Detach();
}

// Обновляет общую логику наземной категории и обрабатывает текущий результат выполнения.
EELControl CELGround::Update(float DeltaTime)
{
	return BehaviorManager.UpdateGround(DeltaTime);
}

// Обновляет выбранного противника и сообщает его сведения схемам.
EELControl CELGround::UpdateTarget(const SELObservation& Observation, float DeltaTime)
{
	BehaviorManager.Clear();
	return EELControl::Native;
}

// Выполняет покадровую часть активной схемы без повторного выбора поведения.
void CELGround::FrameUpdate(float DeltaTime)
{
	if (!HasAttached || !CELSharedDebug::Instance().IsEnabled())
	{
		return;
	}
	SELDebugRecord Record;
	Record.OwnerIdentifier = Adapter->Monster().ID();
	Record.Position = Adapter->Monster().Position();
	Record.GoalPosition = Record.Position;
	const SELObservation* Target = Memory.Find(TargetIdentifier);
	if (Target)
	{
		Record.GoalPosition = Target->Position;
	}
	Record.TargetIdentifier = TargetIdentifier;
	Record.KnownTargets = u32(Memory.GetRecords().size());
	Record.Validation = BehaviorManager.GetValidation().State;
	Record.State = BehaviorManager.GetValidation().Reason;
	Record.IsAttacking = Adapter->Monster().anim().m_tAction == ACT_ATTACK;
	CELSharedDebug::Instance().Publish(std::move(Record));
}
