#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Behaviors/CELFlyingBehaviorBase.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/ExtendedEntityesLogic/Navigation/CELSharedNavigationScheduler.h"
#include "ai/ExtendedEntityesLogic/EntryPoint.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedAnomalies.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELFlyingGeometry.h"
#include "ai/monsters/basemonster/base_monster.h"

// Связывает базовую воздушную схему с проверяемыми маршрутами с необходимыми службами и состоянием владельца.
CELFlyingBehaviorBase::CELFlyingBehaviorBase(IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings, CELFlyingMovement& FlightMovement) : Adapter(OwnerAdapter), Settings(FlightSettings), Movement(FlightMovement)
{
	Navigation.SetClearance(Settings.CollisionRadius, Settings.CollisionCenterHeight);
	Navigation.SetSearch(Settings.SearchCellSize, Settings.SearchNodeLimit, Settings.SearchTimeout);
}

// Подготавливает схему к запуску и сбрасывает состояние предыдущего выполнения.
void CELFlyingBehaviorBase::Enter()
{
	Movement.AcknowledgeObstacle();
	ProgressPosition = Adapter.Monster().Position();
	ProgressTime = Device.dwTimeGlobal;
	RecoveryCount = 0;
	RetryAfter = 0;
	Validation = SELBehaviorValidation();
	RequestedAt = Device.dwTimeGlobal;
	IsRecovering = false;
}

// Обновляет известные сведения о цели для текущей схемы.
void CELFlyingBehaviorBase::SetObservation(const SELObservation* TargetObservation)
{
	HasTarget = TargetObservation != nullptr;
	if (TargetObservation)
	{
		Observation = *TargetObservation;
	}
}

// Отправляет следующую воздушную кривую от конца сохранённого маршрута.
bool CELFlyingBehaviorBase::RequestCurve()
{
	SELNavigationRequest Request;
	Request.IsFlying = true;
	Request.Identity = {ExtendedEntityes::Navigation().GetGeneration(), ExtendedEntityes::OwnerGeneration(Adapter.Monster().ID()), ++PlanGeneration, u16(Adapter.Monster().ID())};
	Request.StartPosition = Movement.HasRoute() ? Movement.EndPosition() : Adapter.Monster().Position();
	Request.StartDirection = Movement.HasRoute() ? Movement.EndDirection() : Movement.Direction();
	CELSharedAnomalies::Snapshot(Adapter.Monster().ID(), Request.Obstacles);
	Movement.AppendObstacles(Request.Obstacles);
	Request.GoalPosition = GoalPosition;
	Request.GoalDirection = GoalDirection;
	Request.StartDirection.normalize_safe();
	Request.GoalDirection.normalize_safe();
	Request.CurveDistance = std::max(Settings.CurveMinimum, std::min(Settings.CurveMaximum, Request.StartPosition.distance_to(Request.GoalPosition) * Settings.CurveRatio));
	RequestedAt = Device.dwTimeGlobal;
	return Navigation.Submit(std::move(Request));
}

// Обновляет базовую воздушную схему с проверяемыми маршрутами и обрабатывает текущий результат выполнения.
void CELFlyingBehaviorBase::Update(float DeltaTime)
{
	ActivatePreparedRoute();
	if (CanPlanCurve() && !Navigation.IsPending() && !Navigation.WasBlocked() && !HasPreparedRoute && s32(Device.dwTimeGlobal - RetryAfter) >= 0)
	{
		ChooseGoal();
		if (!RequestCurve())
		{
			RetryAfter = Device.dwTimeGlobal + Settings.RouteRetryDelay;
		}
	}
	if (Navigation.WasBlocked())
	{
		Validation = {EELBehaviorState::Blocked, "Flight corridor blocked"};
		RetryAfter = Device.dwTimeGlobal + Settings.RouteRetryDelay;
	}
	else
	{
		Validation = {Movement.HasRoute() ? EELBehaviorState::Running : EELBehaviorState::WaitingForResult, Movement.HasRoute() ? "Flying" : Navigation.Status()};
	}
}

// Выполняет покадровую часть активной схемы без повторного выбора поведения.
void CELFlyingBehaviorBase::FrameUpdate(float DeltaTime)
{
	ActivatePreparedRoute();
	if (ProgressPosition.distance_to_sqr(Adapter.Monster().Position()) >= Settings.ProgressDistance * Settings.ProgressDistance)
	{
		ProgressPosition = Adapter.Monster().Position();
		ProgressTime = Device.dwTimeGlobal;
		RecoveryCount = 0;
	}
}

// Возвращает состояние выполнения схемы, включая ожидание, завершение и сбои.
SELBehaviorValidation CELFlyingBehaviorBase::Validate() const
{
	if (!Adapter.Monster().g_Alive())
	{
		return {EELBehaviorState::InvalidTarget, "Owner dead"};
	}
	if (Movement.WasBlocked())
	{
		return {EELBehaviorState::Blocked, "Dynamic obstacle ahead"};
	}
	if (Movement.HasRoute() && Device.dwTimeGlobal - ProgressTime > Settings.ProgressTimeout)
	{
		return {EELBehaviorState::NoProgress, "Flight stalled"};
	}
	if (!Movement.HasRoute() && Device.dwTimeGlobal - ProgressTime > Settings.RouteWaitTimeout)
	{
		return {EELBehaviorState::NoProgress, "Flight route deadline"};
	}
	return Validation;
}

// Выбирает ограниченное восстановление после сбоя либо освобождает схему.
EELRecovery CELFlyingBehaviorBase::Recover(const SELBehaviorValidation& Failure)
{
	if (Failure.State == EELBehaviorState::InvalidTarget || RecoveryCount >= Settings.RecoverySteps)
	{
		return EELRecovery::Release;
	}
	if (IsRecovering && Navigation.IsPending() && Device.dwTimeGlobal - RequestedAt <= Settings.RouteWaitTimeout)
	{
		return EELRecovery::Continue;
	}
	Movement.AcknowledgeObstacle();
	Navigation.Cancel();
	HasPreparedRoute = false;
	IsRecovering = true;
	ProgressTime = Device.dwTimeGlobal;
	++RecoveryCount;
	GoalPosition = Movement.HasRoute() ? Movement.EndPosition() : Adapter.Monster().Position();
	Fvector Departure = Movement.HasRoute() ? Movement.EndDirection() : Movement.Direction();
	Departure.y = 0.f;
	if (Departure.square_magnitude() < EPS_L)
	{
		Departure.set(0.f, 0.f, 1.f);
	}
	Departure.normalize_safe();
	const float Side = Adapter.Monster().ID() % 2 ? 1.f : -1.f;
	const float Clearance = float(std::min(RecoveryCount, Settings.RecoverySteps)) * Settings.RecoveryStep;
	GoalPosition.mad(Departure, Settings.RecoveryForward + Clearance);
	GoalPosition.x += Departure.z * Side * Clearance;
	GoalPosition.z -= Departure.x * Side * Clearance;
	GoalPosition.y += Settings.RecoveryClimb + Clearance;
	const Fbox Bounds = CELFlyingGeometry::AirBounds(Settings.BoundaryMargin, Settings.CollisionRadius, Settings.CollisionCenterHeight);
	clamp(GoalPosition.x, Bounds.min.x, Bounds.max.x);
	clamp(GoalPosition.y, Bounds.min.y, Bounds.max.y);
	clamp(GoalPosition.z, Bounds.min.z, Bounds.max.z);
	GoalDirection.set(Departure.x, .7f, Departure.z).normalize_safe();
	RetryAfter = Device.dwTimeGlobal + Settings.RouteRetryDelay;
	RequestCurve();
	return EELRecovery::Continue;
}

// Завершает активную схему и освобождает временные заявки.
void CELFlyingBehaviorBase::Exit()
{
	Navigation.Cancel();
	HasPreparedRoute = false;
	++PlanGeneration;
}

// Переводит схему в фазу, соответствующую фактически принятому маршруту.
void CELFlyingBehaviorBase::OnRouteActivated()
{
}

// Принимает проверенный маршрут, сохраняя непрерывность движения.
void CELFlyingBehaviorBase::ActivatePreparedRoute()
{
	if (Movement.WasBlocked())
	{
		return;
	}
	if (Navigation.Poll(PreparedRoute))
	{
		HasPreparedRoute = true;
	}
	if (!Movement.HasRoute() && HasPreparedRoute)
	{
		Movement.SetRoute(std::move(PreparedRoute));
		HasPreparedRoute = false;
		if (!IsRecovering)
		{
			OnRouteActivated();
		}
		IsRecovering = false;
	}
}
