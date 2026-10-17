#pragma once
#include <atomic>
#include "../../../../xrCore/FTimer.h"
#include "../Interfaces/IELBehavior.h"

struct SELDebugFrame
{
	u32 OwnerCount = 0;
	u32 WorkerRequests = 0;
	u32 GraphCaptures = 0;
	u32 GeometryChecks = 0;
	u32 WorldGeneration = 0;
	u32 LastWorkerMicroseconds = 0;
	bool IsLogicEnabled = false;
	bool IgnoresActor = false;
};

struct SELDebugRecord
{
	u16 OwnerIdentifier = u16(-1);
	Fvector Position = {0.f, 0.f, 0.f};
	Fvector GoalPosition = {0.f, 0.f, 0.f};
	u32 ExpiresAt = 0;
	float Radius = .5f;
	float Speed = 0.f;
	u16 TargetIdentifier = u16(-1);
	u32 KnownTargets = 0;
	EELBehaviorState Validation = EELBehaviorState::Running;
	bool IsAttacking = false;
	shared_str State;
};

enum class EELDebugEvent : u8
{
	Sound,
	Interest,
	Fear,
	Sharing,
	Call,
	Trail,
	Search,
	Route,
	Hazard
};
struct SELDebugEvent
{
	EELDebugEvent Kind = EELDebugEvent::Sound;
	u16 OwnerIdentifier = u16(-1);
	u16 TargetIdentifier = u16(-1);
	Fvector Start = {0.f, 0.f, 0.f};
	Fvector Finish = {0.f, 0.f, 0.f};
	xr_vector<Fvector> Points;
	float Radius = 0.f;
	u32 ExpiresAt = 0;
	shared_str Label;
};
enum class EELDebugGroup : u8
{
	Ground,
	Flying,
	Shared,
	Count
};
enum class EELDebugStage : u8
{
	Perception,
	Behavior,
	Movement,
	Hearing,
	Sharing,
	World,
	Worker,
	Count
};
struct SELDebugTiming
{
	float AverageMilliseconds = 0.f;
	float PeakMilliseconds = 0.f;
};
struct SELDebugPerformance
{
	SELDebugTiming Groups[u32(EELDebugGroup::Count)];
	SELDebugTiming Main;
	SELDebugTiming Total;
	SELDebugTiming Worker;
};
class CELSharedDebugScope final
{
public:
	CELSharedDebugScope(EELDebugGroup Group, EELDebugStage Stage);
	~CELSharedDebugScope();
	CELSharedDebugScope(const CELSharedDebugScope&) = delete;
	CELSharedDebugScope& operator=(const CELSharedDebugScope&) = delete;

private:
	xr_optional<CTimer> Timer;
	CELSharedDebugScope* Parent = nullptr;
	u64 ChildrenMicroseconds = 0;
	u32 FrameIdentifier = 0;
	EELDebugGroup Group;
	EELDebugStage Stage;
	bool IsActive = false;
};

class CELSharedDebug final
{
public:
	static CELSharedDebug& Instance();
	void SetDrawEnabled(bool IsEnabled);
	void SetStatsEnabled(bool IsEnabled);
	bool IsEnabled() const;
	bool IsDrawEnabled() const;
	bool IsStatsEnabled() const;
	void PublishEvent(SELDebugEvent Event, u32 Lifetime = 1500);
	void RecordTiming(u32 FrameIdentifier, EELDebugGroup Group, EELDebugStage Stage, u64 Microseconds);
	void Publish(SELDebugRecord Record);
	void PublishFrame(const SELDebugFrame& Frame);
	void Draw();
	void Clear();
	void ClearWorld();

private:
	xrCriticalSection QueueLock;
	xr_vector<SELDebugRecord> Records;
	xr_vector<SELDebugEvent> Events;
	struct SFrameTiming
	{
		u32 FrameIdentifier = u32(-1);
		u64 Microseconds[u32(EELDebugGroup::Count)][u32(EELDebugStage::Count)] = {};
	};
	static constexpr u32 TimingFrameCount = 120;
	SFrameTiming Timings[TimingFrameCount];
	u32 TimingStartedAt = u32(-1);
	SELDebugPerformance Performance;
	void UpdatePerformance();
	SELDebugFrame Frame;
	std::atomic<bool> HasDraw{false};
	std::atomic<bool> HasStats{false};
};
