#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedPerceptionProfiles.h"

namespace ExtendedEntityesProfiles
{
// Читает числовой параметр, заменяет недопустимое значение и ограничивает диапазон.
float Number(const CInifile* SystemSettings, const char* Section, const char* LocalKey, const char* SharedKey, float DefaultValue, float Minimum, float Maximum)
{
	float Value = SystemSettings && *SharedKey ? READ_IF_EXISTS(SystemSettings, r_float, "monsters_logic", SharedKey, DefaultValue) : DefaultValue;
	if (!_valid(Value))
	{
		Value = DefaultValue;
	}
	if (*LocalKey)
	{
		Value = READ_IF_EXISTS(pSettings, r_float, Section, LocalKey, Value);
	}
	if (!_valid(Value))
	{
		Value = DefaultValue;
	}
	clamp(Value, Minimum, Maximum);
	return Value;
}

// Читает настройки отдельного типа звуковой реакции.
SELNoiseProfile Noise(const CInifile* SystemSettings, const char* Section, const char* Prefix, const char* SharedPrefix)
{
	SELNoiseProfile Result;
	string128 Key;
	string128 SharedKey;
	xr_sprintf(Key, "%s_enabled", Prefix);
	Result.IsEnabled = READ_IF_EXISTS(pSettings, r_bool, Section, Key, false);
	xr_sprintf(Key, "%s_radius", Prefix);
	Result.Radius = Number(SystemSettings, Section, Key, "", 0.f, 0.f, 500.f);
	xr_sprintf(Key, "%s_panic_chance", Prefix);
	Result.PanicChance = Number(SystemSettings, Section, Key, "", 0.f, 0.f, 1.f);
	xr_sprintf(Key, "%s_attract", Prefix);
	Result.CanAttract = READ_IF_EXISTS(pSettings, r_bool, Section, Key, false);
	xr_sprintf(Key, "%s_interest_enabled", Prefix);
	Result.CanAttract = READ_IF_EXISTS(pSettings, r_bool, Section, Key, Result.CanAttract);
	xr_sprintf(Key, "%s_fear_enabled", Prefix);
	Result.CanFrighten = READ_IF_EXISTS(pSettings, r_bool, Section, Key, Result.PanicChance > 0.f);
	xr_sprintf(Key, "%s_interest_step", Prefix);
	xr_sprintf(SharedKey, "%sInterestStep", SharedPrefix);
	Result.InterestStep = Number(SystemSettings, Section, Key, SharedKey, .25f, 0.f, 1.f);
	xr_sprintf(Key, "%s_interest_threshold", Prefix);
	xr_sprintf(SharedKey, "%sInterestThreshold", SharedPrefix);
	Result.InterestThreshold = Number(SystemSettings, Section, Key, SharedKey, .65f, .01f, 1.f);
	xr_sprintf(Key, "%s_fear_step", Prefix);
	xr_sprintf(SharedKey, "%sFearStep", SharedPrefix);
	Result.FearStep = Number(SystemSettings, Section, Key, SharedKey, .25f, 0.f, 1.f);
	xr_sprintf(Key, "%s_fear_threshold", Prefix);
	xr_sprintf(SharedKey, "%sFearThreshold", SharedPrefix);
	Result.FearThreshold = Number(SystemSettings, Section, Key, SharedKey, .65f, .01f, 1.f);
	if (Result.CanFrighten)
	{
		Result.CanAttract = false;
	}
	return Result;
}
} // namespace ExtendedEntityesProfiles

namespace ExtendedEntityesProfiles
{
// Разбирает список типов шума через запятую в маску разрешённых реакций.
u32 ReadNoiseTypes(const CInifile* SystemSettings, const char* Section, const char* LocalKey, const char* SharedKey, u32 DefaultTypes)
{
	const CInifile* SelectedSettings = nullptr;
	const char* SelectedSection = Section;
	const char* SelectedKey = LocalKey;
	if (pSettings->line_exist(Section, LocalKey))
	{
		SelectedSettings = pSettings;
	}
	else if (SystemSettings && SystemSettings->line_exist("monsters_logic", SharedKey))
	{
		SelectedSettings = SystemSettings;
		SelectedSection = "monsters_logic";
		SelectedKey = SharedKey;
	}
	if (!SelectedSettings)
	{
		return DefaultTypes;
	}
	const char* ConfiguredTypes = SelectedSettings->r_string(SelectedSection, SelectedKey);
	// CInifile represents an explicitly empty value as nullptr, not an absent key.
	if (!ConfiguredTypes || !*ConfiguredTypes)
	{
		return 0;
	}
	const char* TypeNames[] = {"shot", "explosion", "actor_steps", "monster_steps", "jump", "melee", "attack", "injury", "death", "bullet_hit", "breaking", "collision", "talking", "eating", "idle", "ambient", "pickup", "drop", "hide", "take", "use", "empty_click", "reload", "other"};
	static_assert(sizeof(TypeNames) / sizeof(TypeNames[0]) == u32(EELNoiseType::Count));
	u32 Result = 0;
	u32 TypeIndex;
	int TokenIndex;
	const int TokenCount = _GetItemCount(ConfiguredTypes);
	xr_string TypeName;
	bool WasFound;
	for (TokenIndex = 0; TokenIndex < TokenCount; ++TokenIndex)
	{
		_GetItem(ConfiguredTypes, TokenIndex, TypeName);
		if (TypeName.empty())
		{
			continue;
		}
		WasFound = false;
		for (TypeIndex = 0; TypeIndex < u32(EELNoiseType::Count); ++TypeIndex)
		{
			if (TypeName == TypeNames[TypeIndex])
			{
				Result |= 1u << TypeIndex;
				WasFound = true;
				break;
			}
		}
		if (!WasFound)
		{
			Msg("! ExtendedEntityesLogic: [%s] %s: unknown noise type '%s'", Section, LocalKey, TypeName.c_str());
		}
	}
	return Result;
}

// Читает списки интереса и испуга и параметры накопления реакции.
void ReadNoiseReactions(SELPerceptionProfile& Result, const CInifile* SystemSettings, const char* Section)
{
	const u32 ShotMask = ELNoiseTypeMask(EELNoiseType::Shot);
	const u32 ExplosionMask = ELNoiseTypeMask(EELNoiseType::Explosion);
	u32 GeneralMask = (1u << u32(EELNoiseType::Count)) - 1u;
	GeneralMask &= ~(ShotMask | ExplosionMask);
	if (!Result.CanHearActorSteps)
	{
		GeneralMask &= ~(ELNoiseTypeMask(EELNoiseType::ActorSteps) | ELNoiseTypeMask(EELNoiseType::Jump));
	}
	if (!Result.CanHearMonsterSteps)
	{
		GeneralMask &= ~ELNoiseTypeMask(EELNoiseType::MonsterSteps);
	}
	const u32 CombatMask = ELNoiseTypeMask(EELNoiseType::Melee) | ELNoiseTypeMask(EELNoiseType::Attack) | ELNoiseTypeMask(EELNoiseType::Injury) | ELNoiseTypeMask(EELNoiseType::Death) | ELNoiseTypeMask(EELNoiseType::BulletHit);
	const u32 StepMask = ELNoiseTypeMask(EELNoiseType::ActorSteps) | ELNoiseTypeMask(EELNoiseType::MonsterSteps) | ELNoiseTypeMask(EELNoiseType::Jump);
	if (!Result.CanHearCombat)
	{
		GeneralMask &= ~CombatMask;
	}
	if (!Result.CanHearWorld)
	{
		GeneralMask &= CombatMask | StepMask;
	}
	const SELNoiseProfile& GeneralReaction = Result.Shot.CanAttract ? Result.Shot : Result.Explosion;
	u32 InterestTypes = GeneralReaction.CanAttract ? GeneralMask : 0;
	u32 FearTypes = 0;
	if (Result.Shot.IsEnabled)
	{
		if (Result.Shot.CanAttract)
		{
			InterestTypes |= ShotMask;
		}
		if (Result.Shot.CanFrighten)
		{
			FearTypes |= ShotMask;
		}
	}
	if (Result.Explosion.IsEnabled)
	{
		if (Result.Explosion.CanAttract)
		{
			InterestTypes |= ExplosionMask;
		}
		if (Result.Explosion.CanFrighten)
		{
			FearTypes |= ExplosionMask;
		}
	}
	Result.FearNoiseTypes = ReadNoiseTypes(SystemSettings, Section, "monster_noise_fear_types", "MonsterNoiseFearTypes", FearTypes);
	Result.InterestNoiseTypes = ReadNoiseTypes(SystemSettings, Section, "monster_noise_interest_types", "MonsterNoiseInterestTypes", InterestTypes) & ~Result.FearNoiseTypes;
	Result.NoiseFearStep = Number(SystemSettings, Section, "monster_noise_fear_step", "MonsterNoiseFearStep", .25f, 0.f, 1.f);
	Result.NoiseFearThreshold = Number(SystemSettings, Section, "monster_noise_fear_threshold", "MonsterNoiseFearThreshold", .65f, .01f, 1.f);
	Result.NoisePanicChance = Number(SystemSettings, Section, "monster_noise_panic_chance", "MonsterNoisePanicChance", 0.f, 0.f, 1.f);
}

// Читает дальности и множители слуха для разных источников звука.
void ReadHearing(SELPerceptionProfile& Result, const CInifile* SystemSettings, const char* Section)
{
	const char* RatioNames[] = {"shot", "explosion", "step", "combat", "injury", "death", "breaking", "collision", "bullet_hit"};
	string128 Key;
	u32 RatioIndex;
	string128 SharedRatioKey;
	const char* SharedRatioNames[] = {"Shot", "Explosion", "Step", "Combat", "Injury", "Death", "Breaking", "Collision", "BulletHit"};
	for (RatioIndex = 0; RatioIndex < 9; ++RatioIndex)
	{
		xr_sprintf(Key, "monster_%s_hearing_ratio", RatioNames[RatioIndex]);
		xr_sprintf(SharedRatioKey, "Monster%sHearingRatio", SharedRatioNames[RatioIndex]);
		Result.HearingRatios[RatioIndex] = Number(SystemSettings, Section, Key, SharedRatioKey, 1.f, 0.f, 5.f);
	}
	Result.HearingFactor = Number(SystemSettings, Section, "monster_hearing_factor", "", 1.f, 0.f, 5.f);
	Result.SuppressedRatio = Number(SystemSettings, Section, "monster_suppressed_hearing_ratio", "MonsterSuppressedHearingRatio", .3f, 0.f, 1.f);
	Result.MeleeRadius = Number(SystemSettings, Section, "monster_melee_sound_radius", "MonsterMeleeSoundRadius", 5.f, 0.f, 500.f);
	Result.MeleeRatio = Number(SystemSettings, Section, "monster_melee_hearing_ratio", "MonsterMeleeHearingRatio", 1.f, 0.f, 1.f);
	Result.ActorJumpRadius = Number(SystemSettings, Section, "monster_actor_jump_sound_radius", "MonsterActorJumpSoundRadius", 20.f, 0.f, 500.f);
	Result.NoiseInterestInterval = u32(Number(SystemSettings, Section, "monster_noise_interest_interval", "MonsterNoiseInterestInterval", .1f, .05f, 5.f) * 1000.f);
	const char* MovementNames[] = {"walk", "run", "sprint", "high_crouch", "low_crouch"};
	const char* MovementSharedNames[] = {"Walk", "Run", "Sprint", "HighCrouch", "LowCrouch"};
	const float MovementDefaults[] = {7.f, 10.f, 15.f, 5.f, 2.f};
	const char* WeaponNames[] = {"automatic", "sniper", "shotgun", "pistol", "grenade_launcher"};
	const char* WeaponSharedNames[] = {"Automatic", "Sniper", "Shotgun", "Pistol", "GrenadeLauncher"};
	const float WeaponDefaults[] = {.6f, 1.f, .75f, .4f, .25f};
	string128 SharedKey;
	for (RatioIndex = 0; RatioIndex < 5; ++RatioIndex)
	{
		xr_sprintf(Key, "monster_actor_%s_sound_radius", MovementNames[RatioIndex]);
		xr_sprintf(SharedKey, "MonsterActor%sSoundRadius", MovementSharedNames[RatioIndex]);
		Result.ActorMovementRadii[RatioIndex] = Number(SystemSettings, Section, Key, SharedKey, MovementDefaults[RatioIndex], 0.f, 500.f);
		xr_sprintf(Key, "monster_%s_shot_ratio", WeaponNames[RatioIndex]);
		xr_sprintf(SharedKey, "Monster%sShotRatio", WeaponSharedNames[RatioIndex]);
		Result.WeaponRatios[RatioIndex] = Number(SystemSettings, Section, Key, SharedKey, WeaponDefaults[RatioIndex], 0.f, 1.f);
	}
}

// Читает правила обнаружения тел и учёта опасных областей.
void ReadDeaths(SELPerceptionProfile& Result, const CInifile* SystemSettings, const char* Section)
{
	Result.CanInspectBodies = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_corpse_investigation_enabled", SystemSettings ? READ_IF_EXISTS(SystemSettings, r_bool, "monsters_logic", "MonsterCorpseInvestigationEnabled", true) : true);
	Result.CanAvoidDeaths = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_death_hazard_enabled", SystemSettings ? READ_IF_EXISTS(SystemSettings, r_bool, "monsters_logic", "MonsterDeathHazardEnabled", true) : true);
	Result.CanHearDeaths = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_death_alert_enabled", SystemSettings ? READ_IF_EXISTS(SystemSettings, r_bool, "monsters_logic", "MonsterDeathAlertEnabled", true) : true);
	Result.DeathHearingRadius = Number(SystemSettings, Section, "monster_death_hearing_radius", "MonsterDeathHearingRadius", 20.f, 0.f, 100.f);
	Result.BodyDetectionRadius = Number(SystemSettings, Section, "monster_corpse_detection_radius", "MonsterCorpseDetectionRadius", 25.f, 0.f, 100.f);
	Result.BodyInterestTime = u32(Number(SystemSettings, Section, "monster_corpse_interest_time", "MonsterCorpseInterestTime", 90.f, 0.f, 600.f) * 1000.f);
	Result.DeathHazardRadius = Number(SystemSettings, Section, "monster_death_hazard_radius", "MonsterDeathHazardRadius", 12.f, 1.f, 50.f);
	Result.DeathHazardTime = u32(Number(SystemSettings, Section, "monster_death_hazard_time", "MonsterDeathHazardTime", 120.f, 5.f, 600.f) * 1000.f);
	Result.DeathHazardThreshold = u32(Number(SystemSettings, Section, "monster_death_hazard_threshold", "MonsterDeathHazardThreshold", 3.f, 1.f, 20.f));
	Result.BodyInvestigatorLimit = u32(Number(SystemSettings, Section, "monster_corpse_investigators", "MonsterCorpseInvestigators", 2.f, 1.f, 8.f));
	Result.DeathFightQuietTime = u32(Number(SystemSettings, Section, "monster_death_hazard_fight_quiet_time", "MonsterDeathHazardFightQuietTime", 15.f, 1.f, 120.f) * 1000.f);
}

// Разбирает разрешённые стратегии наземного подхода из конфигурации.
void ReadStrategies(SELPerceptionProfile& Result, const char* Section)
{
	const char* StrategyNames[] = {"direct", "flank", "wide", "cautious", "lunge", "charge", "encircle", "pressure", "retreat"};
	static_assert(std::size(StrategyNames) == u32(EELApproachStrategy::Count));
	const char* ConfiguredStrategies = READ_IF_EXISTS(pSettings, r_string, Section, "monster_behavior_strategies", "direct,flank,pressure");
	string64 StrategyName;
	u32 StrategyIndex;
	u32 TokenIndex;
	bool HasStrategy;
	Result.Strategies = 0;
	for (TokenIndex = 0; ConfiguredStrategies && TokenIndex < _GetItemCount(ConfiguredStrategies); ++TokenIndex)
	{
		_GetItem(ConfiguredStrategies, TokenIndex, StrategyName);
		HasStrategy = false;
		for (StrategyIndex = 0; StrategyIndex < u32(EELApproachStrategy::Count); ++StrategyIndex)
		{
			if (!xr_strcmp(StrategyName, StrategyNames[StrategyIndex]))
			{
				Result.Strategies |= 1u << StrategyIndex;
				HasStrategy = true;
			}
		}
		if (!HasStrategy)
		{
			Msg("! [ExtendedEntityesLogic] Unknown approach strategy '%s' in '%s'", StrategyName, Section);
		}
	}
	if (!Result.Strategies)
	{
		Result.Strategies = 1;
	}
}
} // namespace ExtendedEntityesProfiles

// Возвращает кэшированный профиль секции, загружая его при первом обращении.
SELPerceptionProfile CELSharedPerceptionProfiles::Read(const char* Section, const CInifile* SystemSettings)
{
	using namespace ExtendedEntityesProfiles;
	SELPerceptionProfile Result;
	Result.CanUseBehaviors = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_behavior_enabled", (SystemSettings ? READ_IF_EXISTS(SystemSettings, r_bool, "monsters_logic", "MonsterBehaviorEnabled", true) : true));
	Result.IsPassive = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_peaceful", false);
	Result.CanShare = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_enemy_sharing_enabled", true);
	Result.IsWireless = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_enemy_sharing_wireless", false);
	Result.CanTrackLive = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_enemy_tracking_live", false);
	Result.CanCall = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_enemy_call_enabled", true);
	Result.CanAutoFocus = READ_IF_EXISTS(pSettings, r_bool, Section, "autofocus_attacker", false);
	Result.CanPanicOnDamage = READ_IF_EXISTS(pSettings, r_bool, Section, "autopanic_on_invisible_damage", false);
	Result.CanInspect = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_search_sniff", false);
	Result.SharingRadius = Number(SystemSettings, Section, "monster_enemy_sharing_radius", "", 40.0f, 0.0f, 500.0f);
	Result.CloseSharingRatio = Number(SystemSettings, Section, "monster_enemy_sharing_close_ratio", "MonsterEnemySharingCloseRatio", 0.25f, 0.0f, 1.0f);
	Result.CallSharingRatio = Number(SystemSettings, Section, "monster_enemy_sharing_call_ratio", "MonsterEnemySharingCallRatio", 0.6f, 0.0f, 1.0f);
	Result.CloseDetectionRadius = Number(SystemSettings, Section, "monster_enemy_close_detection_radius", "", 0.0f, 0.0f, 100.0f);
	Result.AutoFocusRadius = Number(SystemSettings, Section, "autofocus_max_dist", "", 200.0f, 0.0f, 500.0f);
	Result.CanRecordTrail = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_trail_enabled", SystemSettings ? READ_IF_EXISTS(SystemSettings, r_bool, "monsters_logic", "MonsterTrailEnabled", true) : true);
	Result.TargetCrowdDistance = Number(SystemSettings, Section, "monster_target_crowd_distance", "MonsterTargetCrowdDistance", 20.f, 1.f, 100.f);
	Result.ApproachSpeed = Number(SystemSettings, Section, "monster_approach_speed_estimate", "MonsterApproachSpeedEstimate", 2.f, .1f, 20.f);
	Result.TrailLength = Number(SystemSettings, Section, "monster_trail_length", "MonsterTrailLength", 5.0f, 1.0f, 50.0f);
	Result.ApproachWidth = Number(SystemSettings, Section, "monster_approach_width", "MonsterApproachWidth", 5.0f, 0.0f, 20.0f);
	Result.SearchRadius = Number(SystemSettings, Section, "monster_search_radius", "MonsterSearchRadius", 12.0f, 1.0f, 50.0f);
	Result.SearchAngle = Number(SystemSettings, Section, "monster_search_angle", "MonsterSearchAngle", 60.0f, 10.0f, 180.0f);
	Result.CanHearActorSteps = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_noise_actor_steps_enabled", true);
	Result.CanHearMonsterSteps = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_noise_monster_steps_enabled", true);
	Result.CanHearCombat = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_noise_combat_enabled", true);
	Result.CanHearWorld = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_noise_world_enabled", true);
	Result.NoiseInterestStep = Number(SystemSettings, Section, "monster_noise_interest_step", "MonsterNoiseInterestStep", .25f, 0.f, 1.f);
	Result.NoiseInterestThreshold = Number(SystemSettings, Section, "monster_noise_interest_threshold", "MonsterNoiseInterestThreshold", .65f, .01f, 1.f);
	Result.NoiseSearchTime = u32(Number(SystemSettings, Section, "monster_noise_search_time", "MonsterNoiseSearchTime", 10.f, 1.f, 60.f) * 1000.f);
	Result.NoiseMinPower = Number(SystemSettings, Section, "monster_noise_min_power", "", 0.01f, 0.0f, 1.0f);
	Result.NoiseImmediateChance = Number(SystemSettings, Section, "monster_noise_immediate_interest_chance", "MonsterNoiseImmediateInterestChance", 0.35f, 0.0f, 1.0f);
	Result.NoiseDecay = Number(SystemSettings, Section, "monster_noise_interest_decay", "MonsterNoiseInterestDecay", 0.1f, 0.0f, 1.0f);
	Result.GeneralNoiseRadius = Number(SystemSettings, Section, "monster_sound_reaction_radius", "MonsterSoundReactionRadius", 25.0f, 0.0f, 500.0f);
	Result.SharingIntervalMin = u32(Number(SystemSettings, Section, "", "MonsterEnemySharingIntervalMin", 3.0f, .1f, 3600.f) * 1000.f);
	Result.SharingIntervalMax = u32(Number(SystemSettings, Section, "", "MonsterEnemySharingIntervalMax", 5.0f, .1f, 3600.f) * 1000.f);
	Result.MemoryMin = u32(Number(SystemSettings, Section, "monster_enemy_memory_min", "MonsterEnemyMemoryMin", 30.0f, .1f, 3600.f) * 1000.f);
	Result.MemoryMax = u32(Number(SystemSettings, Section, "monster_enemy_memory_max", "MonsterEnemyMemoryMax", 60.0f, .1f, 3600.f) * 1000.f);
	Result.SearchMin = u32(Number(SystemSettings, Section, "monster_search_time_min", "MonsterSearchTimeMin", 15.0f, .1f, 3600.f) * 1000.f);
	Result.SearchMax = u32(Number(SystemSettings, Section, "monster_search_time_max", "MonsterSearchTimeMax", 25.0f, .1f, 3600.f) * 1000.f);
	Result.ApproachMin = u32(Number(SystemSettings, Section, "monster_approach_time_min", "MonsterApproachTimeMin", 15.f, .1f, 3600.f) * 1000.f);
	Result.ApproachMax = u32(Number(SystemSettings, Section, "monster_approach_time_max", "MonsterApproachTimeMax", 90.0f, .1f, 3600.f) * 1000.f);
	Result.CallCooldown = u32(Number(SystemSettings, Section, "monster_enemy_call_cooldown", "", 30.0f, .1f, 3600.f) * 1000.f);
	Result.NoiseCooldown = u32(Number(SystemSettings, Section, "monster_noise_reaction_cooldown", "", 3.0f, .1f, 3600.f) * 1000.f);
	Result.NoisePanicDuration = u32(Number(SystemSettings, Section, "monster_noise_panic_duration", "", 6.0f, .1f, 3600.f) * 1000.f);
	Result.MemoryMax = std::max(Result.MemoryMin, Result.MemoryMax);
	Result.SharingIntervalMax = std::max(Result.SharingIntervalMin, Result.SharingIntervalMax);
	Result.SearchMax = std::max(Result.SearchMin, Result.SearchMax);
	Result.ApproachMin = std::min(Result.ApproachMin, Result.ApproachMax);
	Result.VisualAcquireTime = u32(Number(SystemSettings, Section, "monster_player_visual_acquire_time", "MonsterPlayerVisualAcquireTime", 400.f, 0.f, 10000.f));
	Result.CanIgnoreFoliage = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_vision_ignore_foliage", false) && (!SystemSettings || READ_IF_EXISTS(SystemSettings, r_bool, "monsters_logic", "EnableMonsterVisionIgnoreFoliage", true));
	Result.CloseVisualRadius = Number(SystemSettings, Section, "monster_close_visual_radius", "MonsterCloseVisualRadius", 10.f, 0.f, 100.f);
	Result.CloseVisualTime = u32(Number(SystemSettings, Section, "monster_close_visual_time", "MonsterCloseVisualTime", 5.f, 0.f, 60.f) * 1000.f);
	Result.NativeAttackDistance = Number(SystemSettings, Section, "monster_enemy_close_attack_distance", "MonsterEnemyCloseAttackDistance", 5.f, .1f, 30.f);
	Result.CanUsePeriphery = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_peripheral_enabled", SystemSettings ? READ_IF_EXISTS(SystemSettings, r_bool, "monsters_logic", "MonsterPeripheralEnabled", true) : true);
	Result.PeripheralAngle = Number(SystemSettings, Section, "monster_peripheral_angle", "MonsterPeripheralAngle", 150.f, 0.f, 360.f);
	Result.PeripheralRadius = Number(SystemSettings, Section, "monster_peripheral_radius", "MonsterPeripheralRadius", 25.f, 0.f, 100.f);
	Result.PeripheralInterval = u32(Number(SystemSettings, Section, "monster_peripheral_interval", "MonsterPeripheralInterval", 1.f, .1f, 10.f) * 1000.f);
	Result.CanAvoidAnomalies = READ_IF_EXISTS(pSettings, r_bool, Section, "monster_anomaly_avoidance_enabled", false);
	Result.AnomalyScanRadius = Number(SystemSettings, Section, "monster_anomaly_scan_radius", "MonsterAnomalyScanRadius", 60.f, 5.f, 150.f);
	Result.AnomalyMargin = Number(SystemSettings, Section, "monster_anomaly_margin", "MonsterAnomalyMargin", 1.f, 0.f, 5.f);
	Result.CallSound = READ_IF_EXISTS(pSettings, r_string, Section, "monster_enemy_call_sound", "");
	Result.Shot = Noise(SystemSettings, Section, "monster_shot_reaction", "MonsterShotReaction");
	Result.Explosion = Noise(SystemSettings, Section, "monster_explosion_reaction", "MonsterExplosionReaction");
	ExtendedEntityesProfiles::ReadHearing(Result, SystemSettings, Section);
	ExtendedEntityesProfiles::ReadNoiseReactions(Result, SystemSettings, Section);
	ExtendedEntityesProfiles::ReadDeaths(Result, SystemSettings, Section);
	ExtendedEntityesProfiles::ReadStrategies(Result, Section);
	return Result;
}
