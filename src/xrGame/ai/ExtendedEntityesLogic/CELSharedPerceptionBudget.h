#pragma once
#include "Interfaces/IELBudget.h"

class CELSharedPerceptionBudget final : public IELBudget
{
public:
	void BeginFrame(u32 FrameIdentifier) override;
	bool TryConsume(u32 OperationCount = 1) override;
	bool TryRay();
	bool TrySpatial();
	bool TrySharing();
	bool TryTransfer();
	void Clear();

private:
	u32 LastFrame = u32(-1);
	u32 RemainingOperations = 0;
	u32 RemainingRays = 0;
	u32 RemainingSpatial = 0;
	u32 RemainingSessions = 0;
	u32 RemainingTransfers = 0;
};
