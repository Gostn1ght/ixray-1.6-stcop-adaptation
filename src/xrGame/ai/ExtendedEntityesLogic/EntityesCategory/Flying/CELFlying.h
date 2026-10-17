#pragma once
#include "../../CoreComponent/CELComponentBase.h"
#include "CELFlyingSettings.h"
#include "CELFlyingMovement.h"
#include "Behaviors/CELFlyingBehaviorManager.h"
class CELFlyingBehaviorBase;

class CELFlying : public CELComponentBase
{
public:
	CELFlying(xr_unique_ptr<IELAdapter> OwnerAdapter, const SELPerceptionProfile& OwnerProfile);
	~CELFlying() override;
	EELControl Update(float DeltaTime) override;
	void FrameUpdate(float DeltaTime) override;
	void Detach() override;

protected:
	void SelectFlight(CELFlyingBehaviorBase& Behavior);
	void ClearBehavior();
	virtual bool IsAttacking() const { return false; }
	virtual Fvector DebugGoal() const;
	const SELFlyingSettings& FlightSettings;
	CELFlyingMovement Movement;
	friend class CELFlyingBehaviorManager;
	CELFlyingBehaviorManager BehaviorManager;
};
