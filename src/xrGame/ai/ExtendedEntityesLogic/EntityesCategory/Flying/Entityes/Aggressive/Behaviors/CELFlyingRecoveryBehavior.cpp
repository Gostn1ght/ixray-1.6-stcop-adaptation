#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Entityes/Aggressive/Behaviors/CELFlyingRecoveryBehavior.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELFlyingGeometry.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/monsters/basemonster/base_monster.h"

// Подготавливает схему к запуску и сбрасывает состояние предыдущего выполнения.
void CELFlyingRecoveryBehavior::Enter()
{
	CELFlyingBehaviorBase::Enter();
	Attempt = 1;
	HasDeparture = false;
}
// Выбирает следующую точку маршрута с учётом текущей фазы поведения.
void CELFlyingRecoveryBehavior::ChooseGoal()
{
	GoalPosition = Movement.HasRoute() ? Movement.EndPosition() : Adapter.Monster().Position();
	Fvector Departure = Movement.HasRoute() ? Movement.EndDirection() : Movement.Direction();
	Departure.y = 0.f;
	if (Departure.square_magnitude() < EPS_L)
	{
		Departure.set(0.f, 0.f, 1.f);
	}
	Departure.normalize_safe();
	const float Clearance = float(Attempt) * Settings.RecoveryStep;
	const float Side = (Adapter.Monster().ID() + Attempt) % 2 ? 1.f : -1.f;
	GoalPosition.mad(Departure, Settings.RecoveryForward + Clearance);
	GoalPosition.x += Departure.z * Side * Clearance;
	GoalPosition.z -= Departure.x * Side * Clearance;
	GoalPosition.y += Settings.RecoveryClimb + Clearance;
	const Fbox Bounds = CELFlyingGeometry::AirBounds(Settings.BoundaryMargin, Settings.CollisionRadius, Settings.CollisionCenterHeight);
	clamp(GoalPosition.x, Bounds.min.x, Bounds.max.x);
	clamp(GoalPosition.y, Bounds.min.y, Bounds.max.y);
	clamp(GoalPosition.z, Bounds.min.z, Bounds.max.z);
	GoalDirection = Departure;
}
// Переводит схему в фазу, соответствующую фактически принятому маршруту.
void CELFlyingRecoveryBehavior::OnRouteActivated()
{
	HasDeparture = true;
}
// Обновляет ограниченную попытками схему выхода из препятствий и обрабатывает текущий результат выполнения.
void CELFlyingRecoveryBehavior::Update(float DeltaTime)
{
	if (!HasDeparture)
	{
		CELFlyingBehaviorBase::Update(DeltaTime);
		return;
	}
	Validation = {Movement.HasRoute() ? EELBehaviorState::Running : EELBehaviorState::Completed, Movement.HasRoute() ? "Leaving blocked flight corridor" : "Flight recovered"};
}
// Выбирает ограниченное восстановление после сбоя либо освобождает схему.
EELRecovery CELFlyingRecoveryBehavior::Recover(const SELBehaviorValidation& Failure)
{
	if (Failure.State == EELBehaviorState::InvalidTarget || Attempt >= Settings.RecoverySteps)
	{
		return EELRecovery::Release;
	}
	Movement.AcknowledgeObstacle();
	Navigation.Cancel();
	HasPreparedRoute = false;
	HasDeparture = false;
	++Attempt;
	ProgressTime = Device.dwTimeGlobal;
	RetryAfter = Device.dwTimeGlobal + Settings.RouteRetryDelay;
	return EELRecovery::Continue;
}
