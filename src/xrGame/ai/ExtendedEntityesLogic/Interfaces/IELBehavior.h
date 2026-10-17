#pragma once

enum class EELBehaviorState : u8
{
	Running,
	Completed,
	WaitingForBudget,
	WaitingForResult,
	Blocked,
	NoProgress,
	InvalidTarget
};

enum class EELRecovery : u8
{
	Continue,
	Restart,
	Release
};

struct SELBehaviorValidation
{
	EELBehaviorState State = EELBehaviorState::Running;
	const char* Reason = "Running";
};

class IELBehavior
{
public:
	virtual ~IELBehavior() = default;
	virtual void Enter() = 0;
	virtual void Update(float DeltaTime) = 0;
	virtual SELBehaviorValidation Validate() const = 0;
	virtual EELRecovery Recover(const SELBehaviorValidation& Validation) = 0;
	virtual void Exit() = 0;
};
