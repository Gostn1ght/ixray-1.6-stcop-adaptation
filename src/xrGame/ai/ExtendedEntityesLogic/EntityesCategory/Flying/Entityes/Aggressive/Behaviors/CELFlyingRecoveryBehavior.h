#pragma once
#include "../../../Behaviors/CELFlyingBehaviorBase.h"

class CELFlyingRecoveryBehavior final : public CELFlyingBehaviorBase
{
public:
	using CELFlyingBehaviorBase::CELFlyingBehaviorBase;
	void Enter() override;
	void Update(float DeltaTime) override;
	EELRecovery Recover(const SELBehaviorValidation& Failure) override;

private:
	void ChooseGoal() override;
	bool CanPlanCurve() const override { return !HasDeparture; }
	void OnRouteActivated() override;
	u32 Attempt = 0;
	bool HasDeparture = false;
};
