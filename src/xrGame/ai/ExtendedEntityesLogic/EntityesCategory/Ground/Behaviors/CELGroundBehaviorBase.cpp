#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/Debug/Debug.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Ground/Behaviors/CELGroundBehaviorBase.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedAnomalies.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedDeaths.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/ExtendedEntityesLogic/CELSharedSettings.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "ai_space.h"
#include "level_graph.h"

// Связывает базовую наземную схему и контроль продвижения с необходимыми службами и состоянием владельца.
CELGroundBehaviorBase::CELGroundBehaviorBase(IELAdapter& OwnerAdapter, const SELPerceptionProfile& OwnerProfile) : Adapter(OwnerAdapter), Profile(OwnerProfile), Movement(OwnerAdapter), Navigation(OwnerAdapter) {}

// Обновляет известные сведения о цели для текущей схемы.
void CELGroundBehaviorBase::SetObservation(const SELObservation& NewObservation)
{
	if (Observation.TargetIdentifier != NewObservation.TargetIdentifier)
	{
		StartedAt = ProgressTime = WaitingSince = Device.dwTimeGlobal;
		RecoveryCount = 0;
		Navigation.Reset();
		Route = SELNavigationResult();
		RouteCursor = 0;
		++PlanGeneration;
	}
	Observation = NewObservation;
}

// Подготавливает схему к запуску и сбрасывает состояние предыдущего выполнения.
void CELGroundBehaviorBase::Enter()
{
	StartedAt = ProgressTime = WaitingSince = Device.dwTimeGlobal;
	ProgressPosition = Adapter.Monster().Position();
	RecoveryCount = 0;
	HasMoveCommand = false;
	Validation = SELBehaviorValidation();
}

// Завершает активную схему и освобождает временные заявки.
void CELGroundBehaviorBase::Exit()
{
	Navigation.Reset();
	Route = SELNavigationResult();
	RouteCursor = 0;
	++PlanGeneration;
	HasMoveCommand = false;
}

// Обновляет контроль продвижения по позиции владельца.
void CELGroundBehaviorBase::TrackProgress()
{
	if (ProgressPosition.distance_to_sqr(Adapter.Monster().Position()) >= .5625f)
	{
		ProgressPosition = Adapter.Monster().Position();
		ProgressTime = WaitingSince = Device.dwTimeGlobal;
		RecoveryCount = 0;
	}
}

// Запрашивает движение к точке и обновляет контроль выполнения.
bool CELGroundBehaviorBase::Move(const Fvector& GoalPosition, u32 GoalVertex, bool IsRunning)
{
	HasMoveCommand = false;
	TrackProgress();
	Navigation.Poll(Route);
	if (Route.HasPath && RouteCursor < Route.Points.size())
	{
		while (RouteCursor + 1 < Route.Points.size() && Adapter.Monster().Position().distance_to_sqr(Route.Points[RouteCursor]) < 1.f)
		{
			++RouteCursor;
		}
		if (CELSharedAnomalies::Crosses(Adapter.Monster().ID(), Adapter.Monster().Position(), Route.Points[RouteCursor]) || CELSharedDeaths::Crosses(Adapter.Monster().ID(), Adapter.Monster().Position(), Route.Points[RouteCursor]))
		{
			Movement.Stop();
			Route = SELNavigationResult();
			Validation = {EELBehaviorState::Blocked, "Маршрут пересекает известную аномалию"};
			return false;
		}

		if (CELSharedDebug::Instance().IsDrawEnabled() && !Route.Points.empty())
		{
			SELDebugEvent Event;
			Event.Kind = EELDebugEvent::Route;
			Event.OwnerIdentifier = Adapter.Monster().ID();
			Event.Start = Adapter.Monster().Position();
			Event.Finish = Route.Points.back();
			Event.Points = Route.Points;
			Event.Radius = .25f;
			Event.Label = "Подготовленный маршрут";
			CELSharedDebug::Instance().PublishEvent(std::move(Event));
		}
		HasMoveCommand = Movement.MoveTo(Route.Points[RouteCursor], Route.Vertices[RouteCursor], IsRunning);
		return HasMoveCommand;
	}
	if (!Navigation.IsPending() && s32(Device.dwTimeGlobal - RetryAfter) >= 0)
	{
		Navigation.Prepare(GoalPosition, GoalVertex, ++PlanGeneration);
		RetryAfter = Device.dwTimeGlobal + 1500;
	}
	Validation = {Navigation.IsPending() ? EELBehaviorState::WaitingForResult : EELBehaviorState::WaitingForBudget, "Ожидание маршрута"};
	// Native path execution receives only prepared adjacent waypoints. Do not start
	// a second full search on the game thread while the snapshot is queued.
	Movement.Stop();
	return true;
}

// Возвращает состояние выполнения схемы, включая ожидание, завершение и сбои.
SELBehaviorValidation CELGroundBehaviorBase::Validate() const
{
	if (!Adapter.CanControl())
	{
		return {EELBehaviorState::InvalidTarget, "Внешнее управление"};
	}
	if (Device.dwTimeGlobal - ProgressTime >= 3000 && HasMoveCommand)
	{
		return {EELBehaviorState::NoProgress, "Нет движения"};
	}
	if (!HasMoveCommand && Device.dwTimeGlobal - WaitingSince >= CELSharedSettings::Work().GroundCaptureTimeout + 500 &&
		(Validation.State == EELBehaviorState::WaitingForResult || Validation.State == EELBehaviorState::WaitingForBudget))
	{
		return {EELBehaviorState::NoProgress, "Истёк срок подготовки пути"};
	}
	return Validation;
}

// Выбирает ограниченное восстановление после сбоя либо освобождает схему.
EELRecovery CELGroundBehaviorBase::Recover(const SELBehaviorValidation& Failure)
{
	if (Failure.State == EELBehaviorState::InvalidTarget)
	{
		return EELRecovery::Release;
	}
	if (++RecoveryCount > 3)
	{
		return EELRecovery::Release;
	}
	ProgressTime = WaitingSince = Device.dwTimeGlobal;
	Navigation.Reset();
	Route = SELNavigationResult();
	RetryAfter = Device.dwTimeGlobal + 1500;
	return EELRecovery::Continue;
}
