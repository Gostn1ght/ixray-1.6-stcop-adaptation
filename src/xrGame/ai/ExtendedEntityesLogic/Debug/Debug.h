#pragma once
#include <atomic>
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

class CELSharedDebug final
{
public:
	static CELSharedDebug& Instance();
	void SetDrawEnabled(bool IsEnabled);
	void SetStatsEnabled(bool IsEnabled);
	bool IsEnabled() const;
	void Publish(SELDebugRecord Record);
	void PublishFrame(const SELDebugFrame& Frame);
	void Draw();
	void Clear();

private:
	xrCriticalSection QueueLock;
	xr_vector<SELDebugRecord> Records;
	SELDebugFrame Frame;
	std::atomic<bool> HasDraw{false};
	std::atomic<bool> HasStats{false};
};
