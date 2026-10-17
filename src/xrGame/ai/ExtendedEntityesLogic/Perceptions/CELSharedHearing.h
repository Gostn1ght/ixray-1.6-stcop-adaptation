#pragma once
#include "CELSharedPerceptionProfiles.h"

enum class EELSoundKind : u8
{
	Native,
	Jump,
	Melee,
	Explosion
};
enum class EELWeaponKind : u8
{
	Automatic,
	Sniper,
	Shotgun,
	Pistol,
	Launcher
};
enum class EELMovementNoise : u8
{
	None,
	Walk,
	Run,
	Sprint,
	HighCrouch,
	LowCrouch
};

struct SELSoundEvent
{
	u16 SourceIdentifier = u16(-1);
	u32 SoundType = 0;
	u32 OccurredAt = 0;
	Fvector Position = {0.f, 0.f, 0.f};
	float Power = 0.f;
	EELSoundKind Kind = EELSoundKind::Native;
	EELWeaponKind WeaponKind = EELWeaponKind::Automatic;
	EELMovementNoise Movement = EELMovementNoise::None;
	bool IsSuppressed = false;
};

struct SELNoiseAttention
{
	u16 SourceIdentifier = u16(-1);
	u32 SoundType = 0;
	EELSoundKind Kind = EELSoundKind::Native;
	u32 LastEvent = 0;
	u32 LastDecay = 0;
	float Interest = 0.f;
	float Fear = 0.f;
};

class CELSharedHearing
{
public:
	static EELNoiseType Classify(const SELSoundEvent& Event);
	static float HearingRadius(const SELSoundEvent& Event, const SELPerceptionProfile& Profile);
	bool Hear(const SELSoundEvent& Event, const Fvector& ListenerPosition, const SELPerceptionProfile& Profile);
	void Clear();
	Fvector AttentionPosition = {0.f, 0.f, 0.f};
	u32 InvestigateUntil = 0;
	u32 PanicUntil = 0;

private:
	xr_vector<SELNoiseAttention> Attention;
	u32 NextReaction = 0;
};
