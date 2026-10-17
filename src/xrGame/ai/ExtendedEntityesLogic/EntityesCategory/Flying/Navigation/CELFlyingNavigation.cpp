#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELFlyingNavigation.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELFlyingGeometry.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELSharedFlyingGraph.h"

// Завершает подготовку и сертификацию воздушного маршрута и освобождает связанные ресурсы.
CELFlyingNavigation::~CELFlyingNavigation()
{
	Cancel();
}
// Задаёт размеры тела для проверки проходимости маршрута.
void CELFlyingNavigation::SetClearance(float CollisionRadius, float BodyCenterHeight)
{
	Radius = CollisionRadius;
	CenterHeight = BodyCenterHeight;
}
// Устанавливает ограничения размера и времени локального поиска.
void CELFlyingNavigation::SetSearch(float CellSize, u32 NodeLimit, u32 Timeout)
{
	SearchCellSize = std::max(CellSize, Radius * 2.f);
	SearchNodeLimit = std::max(NodeLimit, 2u);
	SearchTimeout = Timeout;
}
// Сообщает, ожидается ли ещё подготовка или проверка маршрута.
bool CELFlyingNavigation::IsPending() const
{
	return HasPendingRequest || HasGeometry || HasCapture;
}
// Помещает заявку в очередь с проверкой допустимости и доступного места.
bool CELFlyingNavigation::Submit(SELNavigationRequest Request)
{
	if (IsPending())
	{
		return OriginalRequest.Identity == Request.Identity;
	}
	OriginalRequest = Request;
	HasTriedDetour = false;
	HasBlocked = false;
	return CELNavigationBase::Submit(std::move(Request));
}
// Запускает ограниченный трёхмерный обход после отказа прямой кривой.
bool CELFlyingNavigation::StartDetour()
{
	if (HasTriedDetour)
	{
		return false;
	}
	HasTriedDetour = true;
	HasCapture = CELSharedFlyingGraph::Submit(OriginalRequest, Radius, CenterHeight, SearchCellSize, SearchNodeLimit, SearchTimeout);
	return HasCapture;
}
// Забирает готовый результат либо продолжает подготовку маршрута.
bool CELFlyingNavigation::Poll(SELNavigationResult& Result)
{
	SELNavigationRequest CapturedRequest;
	if (HasCapture)
	{
		const EELGeometryResult CaptureState = CELSharedFlyingGraph::Take(OriginalRequest.Identity, CapturedRequest);
		if (CaptureState == EELGeometryResult::Pending)
		{
			return false;
		}
		HasCapture = false;
		if (CaptureState == EELGeometryResult::Blocked || !CELNavigationBase::Submit(std::move(CapturedRequest)))
		{
			HasBlocked = true;
			return false;
		}
	}
	SELNavigationResult Candidate;
	if (CELNavigationBase::Poll(Candidate))
	{
		GeometryIdentity = Candidate.Identity;
		HasGeometry = Candidate.HasPath && CELFlyingGeometry::Submit(std::move(Candidate), Radius, CenterHeight);
		if (!HasGeometry)
		{
			HasBlocked = !StartDetour();
		}
	}
	if (HasGeometry)
	{
		const EELGeometryResult State = CELFlyingGeometry::Take(GeometryIdentity, Result);
		if (State != EELGeometryResult::Pending)
		{
			HasGeometry = false;
			HasBlocked = State == EELGeometryResult::Blocked && !StartDetour();
			return State == EELGeometryResult::Clear;
		}
	}
	return false;
}
// Отменяет принадлежащую владельцу заявку и очищает промежуточный результат.
void CELFlyingNavigation::Cancel()
{
	CELNavigationBase::Cancel();
	if (HasGeometry)
	{
		CELFlyingGeometry::Cancel(GeometryIdentity);
	}
	if (HasCapture)
	{
		CELSharedFlyingGraph::Cancel(OriginalRequest.Identity);
	}
	HasGeometry = false;
	HasCapture = false;
	HasBlocked = false;
	HasTriedDetour = false;
	OriginalRequest = SELNavigationRequest();
}

// Возвращает краткое объяснение текущего этапа подготовки маршрута.
const char* CELFlyingNavigation::Status() const
{
	if (HasCapture)
	{
		return "Capturing 3D air corridors";
	}
	if (HasGeometry)
	{
		return "Checking flight body clearance";
	}
	if (HasPendingRequest)
	{
		return "Flight worker pending";
	}
	return HasBlocked ? "No certified air corridor" : "Flight request not queued";
}
