#pragma once
#include "../../../Interfaces/IELBehavior.h"
#include "../../../Perceptions/CELSharedMemory.h"
#include "../CELFlyingMovement.h"
#include "../Navigation/CELFlyingNavigation.h"
class IELAdapter;

class CELFlyingBehaviorBase : public IELBehavior
{
public:
	CELFlyingBehaviorBase(IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings, CELFlyingMovement& FlightMovement);
	void Enter() override;
	void Update(float DeltaTime) override;
	SELBehaviorValidation Validate() const override;
	EELRecovery Recover(const SELBehaviorValidation& Validation) override;
	void Exit() override;
	virtual void FrameUpdate(float DeltaTime);
	void SetObservation(const SELObservation* TargetObservation);
	const Fvector& Goal() const { return GoalPosition; }

protected:
	virtual void ChooseGoal() = 0;
	virtual bool CanPlanCurve() const { return true; }
	virtual void OnRouteActivated();
	virtual void ConfigureCurveRequest(SELNavigationRequest& Request) {}
	virtual bool CanActivatePreparedRoute() { return true; }
	bool RequestCurve();
	void ActivatePreparedRoute();
	IELAdapter& Adapter;
	const SELFlyingSettings& Settings;
	CELFlyingMovement& Movement;
	CELFlyingNavigation Navigation;
	SELObservation Observation;
	SELNavigationResult PreparedRoute;
	Fvector GoalPosition = {0.f, 0.f, 0.f};
	Fvector GoalDirection = {0.f, 0.f, 1.f};
	Fvector ProgressPosition = {0.f, 0.f, 0.f};
	u32 PlanGeneration = 0;
	u32 ProgressTime = 0;
	u32 RetryAfter = 0;
	u32 RecoveryCount = 0;
	u32 RequestedAt = 0;
	bool IsRecovering = false;
	bool HasTarget = false;
	bool HasPreparedRoute = false;
	SELBehaviorValidation Validation;
};
