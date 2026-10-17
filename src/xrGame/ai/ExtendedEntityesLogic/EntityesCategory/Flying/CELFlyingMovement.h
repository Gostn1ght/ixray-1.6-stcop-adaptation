#pragma once
#include "../../Interfaces/IELMovement.h"
#include "../../Interfaces/IELNavigation.h"
#include "CELFlyingSettings.h"
class IELAdapter;

class CELFlyingMovement final : public IELMovement
{
public:
	CELFlyingMovement(IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings);
	~CELFlyingMovement() override;
	bool MoveTo(const Fvector& TargetPosition, u32 TargetVertex, bool IsRunning) override;
	void Stop() override;
	void SetRoute(SELNavigationResult Route);
	void RetainRoutePrefix(float Distance);
	void SetContactTarget(u16 Identifier) { ContactTarget = Identifier; }
	void AcknowledgeObstacle() { HasDynamicObstacle = false; }
	bool WasBlocked() const { return HasDynamicObstacle; }
	void AppendObstacles(xr_vector<SELNavigationObstacle>& Obstacles) const;
	const SELFlyingSettings& GetSettings() const { return Settings; }
	void Update(float DeltaTime);
	void SetSpeedScale(float Scale);
	void SetLanding(bool Enabled) { IsLanding = Enabled; }
	bool HasRoute() const;
	float Progress() const;
	float GetSpeed() const { return Velocity.magnitude(); }
	Fvector Direction() const;
	const Fvector& EndPosition() const;
	const Fvector& EndDirection() const;

private:
	void PublishObstacleProbe();
	IELAdapter& Adapter;
	const SELFlyingSettings& Settings;
	SELNavigationResult ActiveRoute;
	Fvector Velocity = {0.f, 0.f, 0.f};
	Fvector FinalDirection = {0.f, 0.f, 1.f};
	u32 Segment = 1;
	float Speed = 0.f;
	float SpeedScale = 1.f;
	bool IsLanding = false;
	bool IsBraking = false;
	bool HasDynamicObstacle = false;
	u16 ContactTarget = u16(-1);
	u32 ObstacleUntil = 0;
	SELNavigationObstacle LastObstacle;
};
