#pragma once
#include "../../../CoreComponent/CELSharedBehaviorManager.h"
#include "../../../Interfaces/IELLogic.h"
#include "CELFlyingCruiseBehavior.h"
class CELFlying;

class CELFlyingBehaviorManager final : public CELSharedBehaviorManager
{
public:
	CELFlyingBehaviorManager(CELFlying& OwnerLogic, IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings, CELFlyingMovement& FlightMovement);
	~CELFlyingBehaviorManager() override;
	EELControl UpdateFlight(float DeltaTime, CELFlyingBehaviorBase* DesiredBehavior = nullptr);
	void SelectFlight(CELFlyingBehaviorBase& Behavior);
	void FrameUpdate(float DeltaTime);
	void Clear();
	Fvector Goal() const;

private:
	CELFlying& Owner;
	CELFlyingMovement& Movement;
	CELFlyingCruiseBehavior CruiseBehavior;
	CELFlyingBehaviorBase* ActiveFlight = nullptr;
};
