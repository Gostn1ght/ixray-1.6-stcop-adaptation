#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/CELSharedPerceptionBudget.h"
#include "ai/ExtendedEntityesLogic/CELSharedSettings.h"

// Восстанавливает общие кадровые квоты восприятия, пространственных запросов и обмена.
void CELSharedPerceptionBudget::BeginFrame(u32 FrameIdentifier)
{
	if (LastFrame != FrameIdentifier)
	{
		LastFrame = FrameIdentifier;
		RemainingOperations = CELSharedSettings::Work().PerceptionOperations;
		RemainingRays = CELSharedSettings::Work().PerceptionRays;
		RemainingSpatial = CELSharedSettings::Work().SpatialQueries;
		RemainingSessions = CELSharedSettings::Work().SharingSessions;
		RemainingTransfers = CELSharedSettings::Work().SharingTransfers;
	}
}

// Резервирует операции из оставшейся порционной квоты.
bool CELSharedPerceptionBudget::TryConsume(u32 OperationCount)
{
	if (OperationCount > RemainingOperations)
	{
		return false;
	}
	RemainingOperations -= OperationCount;
	return true;
}

// Резервирует луч из общего бюджета восприятия.
bool CELSharedPerceptionBudget::TryRay()
{
	if (!RemainingRays)
	{
		return false;
	}
	--RemainingRays;
	return true;
}

// Резервирует один сеанс обмена сведениями.
bool CELSharedPerceptionBudget::TrySharing()
{
	if (!RemainingSessions)
	{
		return false;
	}
	--RemainingSessions;
	return true;
}

// Резервирует доставку одного сообщения об угрозе.
bool CELSharedPerceptionBudget::TryTransfer()
{
	if (!RemainingTransfers)
	{
		return false;
	}
	--RemainingTransfers;
	return true;
}

// Сбрасывает накопленные данные и незавершённую работу подсистемы.
void CELSharedPerceptionBudget::Clear()
{
	LastFrame = u32(-1);
	RemainingOperations = RemainingRays = RemainingSpatial = RemainingSessions = RemainingTransfers = 0;
}

// Резервирует пространственный запрос в текущем кадре.
bool CELSharedPerceptionBudget::TrySpatial()
{
	if (!RemainingSpatial)
	{
		return false;
	}
	--RemainingSpatial;
	return true;
}
