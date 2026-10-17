#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedHearing.h"
#include "../xrSound/ai_sounds.h"

// Определяет тип звукового события по игровым флагам.
EELNoiseType CELSharedHearing::Classify(const SELSoundEvent& Event)
{
	if (Event.Kind == EELSoundKind::Explosion || (Event.SoundType & SOUND_TYPE_OBJECT_EXPLODING))
	{
		return EELNoiseType::Explosion;
	}
	if (Event.Kind == EELSoundKind::Jump)
	{
		return EELNoiseType::Jump;
	}
	if (Event.Kind == EELSoundKind::Melee)
	{
		return EELNoiseType::Melee;
	}
	if (Event.SoundType & SOUND_TYPE_SHOOTING)
	{
		return EELNoiseType::Shot;
	}
	if (Event.SoundType & SOUND_TYPE_STEP)
	{
		return Event.Movement != EELMovementNoise::None ? EELNoiseType::ActorSteps : EELNoiseType::MonsterSteps;
	}
	const u32 NativeTypes[] = {SOUND_TYPE_ATTACKING, SOUND_TYPE_INJURING, SOUND_TYPE_DYING, SOUND_TYPE_BULLET_HIT, SOUND_TYPE_OBJECT_BREAKING, SOUND_TYPE_OBJECT_COLLIDING, SOUND_TYPE_TALKING, SOUND_TYPE_EATING, SOUND_TYPE_IDLE, SOUND_TYPE_AMBIENT, SOUND_TYPE_PICKING_UP, SOUND_TYPE_DROPPING, SOUND_TYPE_HIDING, SOUND_TYPE_TAKING, SOUND_TYPE_USING, SOUND_TYPE_EMPTY_CLICKING, SOUND_TYPE_RECHARGING};
	static_assert(sizeof(NativeTypes) / sizeof(NativeTypes[0]) == u32(EELNoiseType::Other) - u32(EELNoiseType::Attack));
	u32 TypeIndex;
	for (TypeIndex = 0; TypeIndex < sizeof(NativeTypes) / sizeof(NativeTypes[0]); ++TypeIndex)
	{
		if (Event.SoundType & NativeTypes[TypeIndex])
		{
			return EELNoiseType(u32(EELNoiseType::Attack) + TypeIndex);
		}
	}
	return EELNoiseType::Other;
}

// Рассчитывает дальность слуха по типу звука и профилю получателя.
float CELSharedHearing::HearingRadius(const SELSoundEvent& Event, const SELPerceptionProfile& Profile)
{
	const EELNoiseType Type = Classify(Event);
	u32 RatioIndex = 3;
	switch (Type)
	{
		case EELNoiseType::Shot: RatioIndex = 0; break;
		case EELNoiseType::Explosion: RatioIndex = 1; break;
		case EELNoiseType::ActorSteps:
		case EELNoiseType::MonsterSteps:
		case EELNoiseType::Jump: RatioIndex = 2; break;
		case EELNoiseType::Injury: RatioIndex = 4; break;
		case EELNoiseType::Death: RatioIndex = 5; break;
		case EELNoiseType::Breaking: RatioIndex = 6; break;
		case EELNoiseType::Collision: RatioIndex = 7; break;
		case EELNoiseType::BulletHit: RatioIndex = 8; break;
		default: break;
	}
	const float HearingRatio = Profile.HearingRatios[RatioIndex];
	if (HearingRatio <= 0.f)
	{
		return 0.f;
	}
	const bool HasOwnRadius = Type == EELNoiseType::Jump || Type == EELNoiseType::Melee || Event.Movement != EELMovementNoise::None;
	float Radius = Type == EELNoiseType::Shot ? Profile.Shot.Radius : Type == EELNoiseType::Explosion ? Profile.Explosion.Radius
																									  : Profile.GeneralNoiseRadius;
	if (Type == EELNoiseType::Jump)
	{
		Radius = Profile.ActorJumpRadius;
	}
	else if (Type == EELNoiseType::Melee)
	{
		Radius = Profile.MeleeRadius * Profile.MeleeRatio;
	}
	else if (Event.Movement != EELMovementNoise::None)
	{
		Radius = Profile.ActorMovementRadii[u32(Event.Movement) - 1];
	}
	else if (Type == EELNoiseType::Shot)
	{
		Radius *= Profile.WeaponRatios[u32(Event.WeaponKind)];
	}
	if (Event.IsSuppressed)
	{
		Radius *= Profile.SuppressedRatio;
	}
	return Radius * (HasOwnRadius ? 1.f : HearingRatio) * Profile.HearingFactor;
}

// Обрабатывает принятое звуковое событие по настроенным правилам реакции.
bool CELSharedHearing::Hear(const SELSoundEvent& Event, const Fvector& ListenerPosition, const SELPerceptionProfile& Profile)
{
	if (!Event.SoundType || Event.SoundType == u32(-1) || !_valid(Event.Position))
	{
		return false;
	}
	const EELNoiseType Type = Classify(Event);
	const u32 TypeMask = ELNoiseTypeMask(Type);
	const bool CanFrighten = (Profile.FearNoiseTypes & TypeMask) != 0;
	const bool CanAttract = !CanFrighten && (Profile.InterestNoiseTypes & TypeMask) != 0;
	if (!CanAttract && !CanFrighten)
	{
		return false;
	}
	const SELNoiseProfile* Noise = Type == EELNoiseType::Shot ? &Profile.Shot : Type == EELNoiseType::Explosion ? &Profile.Explosion
																												: nullptr;
	const float InterestStep = Noise ? Noise->InterestStep : Profile.NoiseInterestStep;
	const float InterestThreshold = Noise ? Noise->InterestThreshold : Profile.NoiseInterestThreshold;
	const float FearStep = Noise ? Noise->FearStep : Profile.NoiseFearStep;
	const float FearThreshold = Noise ? Noise->FearThreshold : Profile.NoiseFearThreshold;
	const float PanicChance = Noise ? Noise->PanicChance : Profile.NoisePanicChance;
	const float Radius = HearingRadius(Event, Profile);
	if (Radius <= 0.f || ListenerPosition.distance_to_sqr(Event.Position) > Radius * Radius)
	{
		return false;
	}
	SELNoiseAttention* SourceAttention = nullptr;
	xr_vector<SELNoiseAttention>::iterator AttentionIterator;
	for (AttentionIterator = Attention.begin(); AttentionIterator != Attention.end(); ++AttentionIterator)
	{
		if (AttentionIterator->SourceIdentifier == Event.SourceIdentifier && AttentionIterator->SoundType == Event.SoundType && AttentionIterator->Kind == Event.Kind)
		{
			SourceAttention = &*AttentionIterator;
			break;
		}
	}
	if (!SourceAttention)
	{
		if (Attention.size() >= 8)
		{
			Attention.erase(Attention.begin());
		}
		Attention.emplace_back();
		SourceAttention = &Attention.back();
		SourceAttention->SourceIdentifier = Event.SourceIdentifier;
		SourceAttention->SoundType = Event.SoundType;
		SourceAttention->Kind = Event.Kind;
		SourceAttention->LastDecay = Event.OccurredAt;
	}
	if (SourceAttention->LastEvent && Event.OccurredAt - SourceAttention->LastEvent < Profile.NoiseInterestInterval)
	{
		return true;
	}
	const float Decay = float(Event.OccurredAt - SourceAttention->LastDecay) * .001f * Profile.NoiseDecay;
	SourceAttention->Interest = std::max(0.f, SourceAttention->Interest - Decay);
	SourceAttention->Fear = std::max(0.f, SourceAttention->Fear - Decay);
	SourceAttention->LastEvent = SourceAttention->LastDecay = Event.OccurredAt;
	if (CanFrighten)
	{
		SourceAttention->Fear += FearStep;
	}
	if (CanAttract)
	{
		SourceAttention->Interest += InterestStep;
	}
	if (s32(Event.OccurredAt - NextReaction) < 0)
	{
		return true;
	}
	if (CanFrighten && (SourceAttention->Fear >= FearThreshold || Random.randF() < PanicChance))
	{
		AttentionPosition = Event.Position;
		PanicUntil = Event.OccurredAt + Profile.NoisePanicDuration;
		InvestigateUntil = 0;
		NextReaction = Event.OccurredAt + Profile.NoiseCooldown;
		SourceAttention->Fear = 0.f;
	}
	else if (CanAttract && s32(Event.OccurredAt - PanicUntil) >= 0 && (SourceAttention->Interest >= InterestThreshold || Random.randF() < Profile.NoiseImmediateChance))
	{
		AttentionPosition = Event.Position;
		InvestigateUntil = Event.OccurredAt + Profile.NoiseSearchTime;
		NextReaction = Event.OccurredAt + Profile.NoiseCooldown;
		SourceAttention->Interest = 0.f;
	}
	return true;
}

// Сбрасывает накопленные данные и незавершённую работу подсистемы.
void CELSharedHearing::Clear()
{
	Attention.clear();
	InvestigateUntil = PanicUntil = NextReaction = 0;
}
