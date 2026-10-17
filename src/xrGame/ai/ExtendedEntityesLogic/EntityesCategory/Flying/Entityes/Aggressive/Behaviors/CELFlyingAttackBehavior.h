#pragma once
#include "../../../Behaviors/CELFlyingBehaviorBase.h"
#include "../CELFlyingAttack.h"

class CELFlyingAttackAnimation;

enum class EELAttackPhase : u8
{
	Climb,
	Approach,
	Dive,
	Exit
};

class CELFlyingAttackBehavior final : public CELFlyingBehaviorBase
{
public:
	CELFlyingAttackBehavior(IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings, CELFlyingMovement& FlightMovement, CELFlyingAttackAnimation& FlightAnimation);
	void Exit() override;
	void Enter() override;
	EELRecovery Recover(const SELBehaviorValidation& Failure) override;
	void SetObservation(const SELObservation* TargetObservation);
	void FrameUpdate(float DeltaTime) override;
	bool IsAttacking() const { return Attack.IsAttacking(); }

private:
	void ChooseGoal() override;
	void OnRouteActivated() override;
	CELFlyingAttackAnimation& Animation;
	CELFlyingAttack Attack;
	EELAttackPhase Phase = EELAttackPhase::Climb;
	EELAttackPhase PlannedPhase = EELAttackPhase::Climb;
	EELAttackPhase ActivePhase = EELAttackPhase::Climb;
	float OrbitAngle = 0.f;
};
