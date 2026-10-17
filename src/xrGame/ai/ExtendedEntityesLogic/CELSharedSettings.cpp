#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/CELSharedSettings.h"
#include "ai/ExtendedEntityesLogic/Debug/DebugCommands.h"
#include <atomic>

namespace ExtendedEntityesSettings
{
xr_unique_ptr<CInifile> SystemSettings;
xr_map<shared_str, SELPerceptionProfile> Profiles;
std::atomic<int> OverrideValue{-1};
std::atomic<u32> ControlRevision{1};
xrCriticalSection SettingsLock;
bool IsInitialized = false;
bool IsConfiguredEnabled = false;
bool CanShareFriends = true;
bool CanDistribute = true;
bool CanCall = true;
SELSharedWorkSettings WorkSettings;
} // namespace ExtendedEntityesSettings

// Загружает настройки и подготавливает состояние перед использованием.
void CELSharedSettings::Initialize()
{
	using namespace ExtendedEntityesSettings;
	xrCriticalSectionGuard Guard(SettingsLock);
	if (IsInitialized)
	{
		return;
	}
	CELSharedDebugCommands::Synchronize();
	string_path FileName;
	FS.update_path(FileName, _game_config_, "new_monsters_logic.ltx");
	if (FS.exist(FileName))
	{
		SystemSettings = xr_make_unique<CInifile>(FileName, true);
		IsConfiguredEnabled = READ_IF_EXISTS(EngineExternal().GetIniFile(), r_bool, "gameplay", "UseDeathOrDieMonstersLogic", false);
		CanShareFriends = READ_IF_EXISTS(SystemSettings.get(), r_bool, "monsters_logic", "EnableMonsterFriendlyEnemySharing", true);
		CanDistribute = READ_IF_EXISTS(SystemSettings.get(), r_bool, "monsters_logic", "EnableMonsterDistributedTargeting", true);
		WorkSettings.PerceptionOperations = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedPerceptionOperationsPerFrame", 384u);
		WorkSettings.PerceptionRays = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "MonsterBehaviorRaysPerFrame", 16u);
		WorkSettings.SpatialQueries = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "MonsterBehaviorQueriesPerFrame", 2u);
		WorkSettings.SharingSessions = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedSharingSessionsPerFrame", 12u);
		WorkSettings.TargetAssignments = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedTargetAssignmentsPerFrame", 64u);
		WorkSettings.SharingTransfers = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedSharingTransfersPerFrame", 32u);
		WorkSettings.NavigationCaptureNodes = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "MonsterBehaviorNodesPerFrame", 72u);
		WorkSettings.NavigationSteps = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedNavigationStepsPerFrame", 256u);
		WorkSettings.NavigationSliceMicroseconds = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedNavigationSliceMicroseconds", 1500u);
		WorkSettings.FlightDynamicChecks = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedFlightDynamicChecksPerFrame", 4u);
		WorkSettings.FlightGeometryRays = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedFlightGeometryRaysPerFrame", 192u);
		WorkSettings.NavigationTaskLimit = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedNavigationTaskLimit", 256u);
		WorkSettings.NavigationGraphNodeLimit = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedNavigationGraphNodeLimit", 16384u);
		WorkSettings.GroundCaptureTaskLimit = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedGroundCaptureTaskLimit", 128u);
		WorkSettings.GroundCaptureNodeLimit = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedGroundCaptureNodeLimit", 4096u);
		WorkSettings.GroundCaptureTimeout = READ_IF_EXISTS(SystemSettings.get(), r_u32, "monsters_logic", "ExtendedGroundCaptureTimeoutMs", 20000u);
		clamp(WorkSettings.NavigationTaskLimit, 16u, 4096u);
		clamp(WorkSettings.NavigationGraphNodeLimit, 64u, 65536u);
		clamp(WorkSettings.GroundCaptureTaskLimit, 16u, WorkSettings.NavigationTaskLimit);
		clamp(WorkSettings.GroundCaptureNodeLimit, 16u, WorkSettings.NavigationGraphNodeLimit);
		clamp(WorkSettings.GroundCaptureTimeout, 1000u, 120000u);
		clamp(WorkSettings.PerceptionOperations, 16u, 4096u);
		clamp(WorkSettings.PerceptionRays, 1u, 64u);
		clamp(WorkSettings.SpatialQueries, 1u, 8u);
		clamp(WorkSettings.SharingSessions, 1u, 64u);
		clamp(WorkSettings.TargetAssignments, 1u, 1024u);
		clamp(WorkSettings.SharingTransfers, 1u, 256u);
		clamp(WorkSettings.NavigationCaptureNodes, 8u, 256u);
		clamp(WorkSettings.NavigationSteps, 16u, 2048u);
		clamp(WorkSettings.NavigationSliceMicroseconds, 100u, 5000u);
		clamp(WorkSettings.FlightGeometryRays, 16u, 512u);
		clamp(WorkSettings.FlightDynamicChecks, 1u, std::max(1u, WorkSettings.FlightGeometryRays / 18u));
		CanCall = READ_IF_EXISTS(SystemSettings.get(), r_bool, "monsters_logic", "EnableMonsterEnemySharingCallSounds", true);
	}
	IsInitialized = true;
}

// Возвращает общий кэш профиля, прочитанного из секции сущности.
const SELPerceptionProfile& CELSharedSettings::Profile(const char* Section)
{
	Initialize();
	xrCriticalSectionGuard Guard(ExtendedEntityesSettings::SettingsLock);
	xr_map<shared_str, SELPerceptionProfile>::iterator ProfileIterator = ExtendedEntityesSettings::Profiles.find(Section);
	if (ProfileIterator == ExtendedEntityesSettings::Profiles.end())
	{
		ProfileIterator = ExtendedEntityesSettings::Profiles.emplace(shared_str(Section), CELSharedPerceptionProfiles::Read(Section, ExtendedEntityesSettings::SystemSettings.get())).first;
	}
	return ProfileIterator->second;
}

// Возвращает общие ограничения порционных расчётов.
const SELSharedWorkSettings& CELSharedSettings::Work()
{
	return ExtendedEntityesSettings::WorkSettings;
}

// Сообщает, включена ли соответствующая подсистема.
bool CELSharedSettings::IsEnabled()
{
	const int OverrideValue = ExtendedEntityesSettings::OverrideValue.load(std::memory_order_relaxed);
	return OverrideValue < 0 ? ExtendedEntityesSettings::IsConfiguredEnabled : OverrideValue != 0;
}

// Запоминает консольный режим и меняет ревизию управления.
void CELSharedSettings::SetEnabled(int OverrideValue)
{
	clamp(OverrideValue, -1, 1);
	ExtendedEntityesSettings::OverrideValue.store(OverrideValue, std::memory_order_relaxed);
	ExtendedEntityesSettings::ControlRevision.fetch_add(1, std::memory_order_relaxed);
}

// Возвращает ревизию, по которой компоненты сбрасывают старое управление.
u32 CELSharedSettings::Revision()
{
	return ExtendedEntityesSettings::ControlRevision.load(std::memory_order_relaxed);
}

// Возвращает выбранный консольный режим: конфиг, выключено или включено.
int CELSharedSettings::GetOverride()
{
	return ExtendedEntityesSettings::OverrideValue.load(std::memory_order_relaxed);
}
// Возвращает разрешение обмениваться сведениями с дружественными видами.
bool CELSharedSettings::CanShareWithFriends()
{
	return ExtendedEntityesSettings::CanShareFriends;
}
// Возвращает разрешение распределять противников между союзниками.
bool CELSharedSettings::CanDistributeTargets()
{
	return ExtendedEntityesSettings::CanDistribute;
}
// Возвращает разрешение звуковых кличей при передаче сведений.
bool CELSharedSettings::CanPlayCalls()
{
	return ExtendedEntityesSettings::CanCall;
}

// Сбрасывает накопленные данные и незавершённую работу подсистемы.
void CELSharedSettings::Clear()
{
	ExtendedEntityesSettings::Profiles.clear();
	ExtendedEntityesSettings::SystemSettings.reset();
	ExtendedEntityesSettings::WorkSettings = SELSharedWorkSettings();
	ExtendedEntityesSettings::IsInitialized = false;
	ExtendedEntityesSettings::IsConfiguredEnabled = false;
}
