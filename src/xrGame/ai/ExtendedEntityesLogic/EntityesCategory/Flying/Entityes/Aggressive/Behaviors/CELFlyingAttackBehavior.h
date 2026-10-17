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
	void Update(float DeltaTime) override;
	SELBehaviorValidation Validate() const override;
	bool IsAttacking() const { return Attack.IsAttacking(); }

private:
	void ChooseGoal() override;
	void OnRouteActivated() override;
	bool CanActivatePreparedRoute() override;
	void ConfigureCurveRequest(SELNavigationRequest& Request) override;
	void RefreshTarget();
	void CorrectDive();
	void ChooseDeparture(const Fvector& Origin);
	void ChooseApproach(const Fvector& Origin);
	void ChooseContact(const Fvector& Origin);
	CELFlyingAttackAnimation& Animation;
	CELFlyingAttack Attack;
	EELAttackPhase Phase = EELAttackPhase::Climb;
	EELAttackPhase PlannedPhase = EELAttackPhase::Climb;
	EELAttackPhase ActivePhase = EELAttackPhase::Climb;
	Fvector AimPosition = {0.f, 0.f, 0.f};
	Fvector TargetVelocity = {0.f, 0.f, 0.f};
	Fvector PlannedTargetPosition = {0.f, 0.f, 0.f};
	Fvector ActiveTargetPosition = {0.f, 0.f, 0.f};
	u32 TargetSampleAt = 0;
	u32 LastCorrectionAt = 0;
	float OrbitAngle = 0.f;
	float TurnSide = 1.f;
	float PassRadius = 0.f;
	float PassHeight = 0.f;
	bool HasVisibleTarget = false;
	bool HasHitPass = false;
	bool IsDivePass = true;
};
