#pragma once
#include "CELFlyingBehaviorBase.h"

class CELFlyingCruiseBehavior final : public CELFlyingBehaviorBase
{
public:
	using CELFlyingBehaviorBase::CELFlyingBehaviorBase;
	void Enter() override;
	void SetAttention(const Fvector& Position, bool IsEscape);
	void ClearAttention();

private:
	void ChooseGoal() override;
	Fvector OrbitCenter = {0.f, 0.f, 0.f};
	float OrbitAngle = 0.f;
	Fvector AttentionPosition = {0.f, 0.f, 0.f};
	bool HasAttention = false;
	bool IsEscaping = false;
};
