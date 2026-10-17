#pragma once
#include "../../CELBaseAdapter.h"

class CELFlyingAdapter final : public CELBaseAdapter
{
public:
	using CELBaseAdapter::CELBaseAdapter;
	bool CanReach(const Fvector& TargetPosition, u32 TargetVertex) const override;
	void ReleaseControl() override;
};
