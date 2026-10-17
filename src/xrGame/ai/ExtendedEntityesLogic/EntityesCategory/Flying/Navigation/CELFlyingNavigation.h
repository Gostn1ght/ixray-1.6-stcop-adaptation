#pragma once
#include "../../../Navigation/CELNavigationBase.h"

class CELFlyingNavigation final : public CELNavigationBase
{
public:
	~CELFlyingNavigation() override;
	bool Submit(SELNavigationRequest Request) override;
	bool Poll(SELNavigationResult& Result) override;
	void Cancel() override;
	bool IsPending() const override;
	void SetClearance(float CollisionRadius, float CenterHeight);
	void SetSearch(float CellSize, u32 NodeLimit, u32 Timeout);
	const char* Status() const;
	bool WasBlocked() const { return HasBlocked; }

private:
	bool StartDetour();
	SELNavigationRequest OriginalRequest;
	float SearchCellSize = 4.f;
	u32 SearchNodeLimit = 128;
	u32 SearchTimeout = 8000;
	bool HasCapture = false;
	bool HasTriedDetour = false;
	float Radius = .35f;
	float CenterHeight = .4f;
	SELNavigationIdentity GeometryIdentity;
	bool HasGeometry = false;
	bool HasBlocked = false;
};
