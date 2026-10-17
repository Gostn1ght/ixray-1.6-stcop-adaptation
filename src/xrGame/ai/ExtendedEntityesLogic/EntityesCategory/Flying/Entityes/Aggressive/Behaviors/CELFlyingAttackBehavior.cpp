#include "stdafx.h"
#include <cmath>
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Entityes/Aggressive/CELFlyingAttackAnimation.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Entityes/Aggressive/Behaviors/CELFlyingAttackBehavior.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/ExtendedEntityesLogic/EntryPoint.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELFlyingGeometry.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "Level.h"

// Связывает заход, пикирование и выход из атаки с необходимыми службами и состоянием владельца.
CELFlyingAttackBehavior::CELFlyingAttackBehavior(IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings, CELFlyingMovement& FlightMovement, CELFlyingAttackAnimation& FlightAnimation) : CELFlyingBehaviorBase(OwnerAdapter, FlightSettings, FlightMovement), Animation(FlightAnimation) {}
// Завершает активную схему и освобождает временные заявки.
void CELFlyingAttackBehavior::Exit()
{
	Movement.SetContactTarget(u16(-1));
	Animation.Fly();
	CELFlyingBehaviorBase::Exit();
}
// Подготавливает схему к запуску и сбрасывает состояние предыдущего выполнения.
void CELFlyingAttackBehavior::Enter()
{
	CELFlyingBehaviorBase::Enter();
	Animation.Fly();
	Phase = PlannedPhase = ActivePhase = EELAttackPhase::Climb;
}

// Обновляет известные сведения о цели для текущей схемы.
void CELFlyingAttackBehavior::SetObservation(const SELObservation* TargetObservation)
{
	if (TargetObservation && HasTarget && Observation.TargetIdentifier != TargetObservation->TargetIdentifier)
	{
		Movement.SetContactTarget(u16(-1));
		Movement.RetainRoutePrefix(std::max(Settings.CollisionRadius * 2.f, Movement.GetSpeed() * Settings.InterruptLookahead));
		Navigation.Cancel();
		HasPreparedRoute = false;
		Phase = PlannedPhase = ActivePhase = EELAttackPhase::Climb;
	}
	CELFlyingBehaviorBase::SetObservation(TargetObservation);
}

// Выполняет покадровую часть активной схемы без повторного выбора поведения.
void CELFlyingAttackBehavior::FrameUpdate(float DeltaTime)
{
	// The final dive point must be tested before activating a prepared exit route.
	if (HasTarget && ActivePhase == EELAttackPhase::Dive)
	{
		Attack.TryHit(Adapter, Settings, Observation.TargetIdentifier);
	}
	CELFlyingBehaviorBase::FrameUpdate(DeltaTime);
}

// Выбирает следующую точку маршрута с учётом текущей фазы поведения.
void CELFlyingAttackBehavior::ChooseGoal()
{
	PlannedPhase = Phase;
	const Fvector Origin = Movement.HasRoute() ? Movement.EndPosition() : Adapter.Monster().Position();
	const Fvector Tangent = Movement.HasRoute() ? Movement.EndDirection() : Movement.Direction();
	float SurfaceHeight = Observation.Position.y;
	CELFlyingGeometry::Surface(Observation.Position, SurfaceHeight);
	GoalPosition = Observation.Position;
	switch (Phase)
	{
		case EELAttackPhase::Climb:
		case EELAttackPhase::Exit:
			OrbitAngle += PI_DIV_2;
			GoalPosition.x += std::sin(OrbitAngle) * Settings.OrbitRadius;
			GoalPosition.z += std::cos(OrbitAngle) * Settings.OrbitRadius;
			GoalPosition.y = std::max(SurfaceHeight + Settings.Altitude, Origin.y);
			GoalDirection.set(std::cos(OrbitAngle), Settings.AttackPulloutPitch, -std::sin(OrbitAngle));
			break;
		case EELAttackPhase::Approach:
			GoalPosition.mad(Observation.Position, Tangent, -Settings.OrbitRadius * Settings.AttackApproachRatio);
			GoalPosition.y = SurfaceHeight + Settings.Altitude * Settings.AttackApproachHeightRatio;
			GoalDirection.sub(Observation.Position, GoalPosition).normalize_safe();
			break;
		case EELAttackPhase::Dive:
			GoalPosition.y = std::max(Observation.Position.y + Settings.CollisionRadius + Settings.GroundClearance, SurfaceHeight + Settings.CollisionRadius + Settings.GroundClearance);
			// Pull out horizontally at contact; an upward end tangent would put its preceding control point below the terrain.
			GoalDirection.set(Tangent.x, 0.f, Tangent.z).normalize_safe();
			break;
	}
	const Fbox Bounds = CELFlyingGeometry::AirBounds(Settings.BoundaryMargin, Settings.CollisionRadius, Settings.CollisionCenterHeight);
	clamp(GoalPosition.x, Bounds.min.x, Bounds.max.x);
	clamp(GoalPosition.y, Bounds.min.y, Bounds.max.y);
	clamp(GoalPosition.z, Bounds.min.z, Bounds.max.z);
}

// Переводит схему в фазу, соответствующую фактически принятому маршруту.
void CELFlyingAttackBehavior::OnRouteActivated()
{
	ActivePhase = PlannedPhase;
	Movement.SetContactTarget(ActivePhase == EELAttackPhase::Dive ? Observation.TargetIdentifier : u16(-1));
	if (ActivePhase == EELAttackPhase::Dive)
	{
		Animation.Attack();
	}
	else
	{
		Animation.Fly();
	}
	switch (ActivePhase)
	{
		case EELAttackPhase::Climb:
		case EELAttackPhase::Exit:
			Phase = EELAttackPhase::Approach;
			break;
		case EELAttackPhase::Approach:
			Phase = EELAttackPhase::Dive;
			break;
		case EELAttackPhase::Dive:
			Phase = EELAttackPhase::Exit;
			break;
	}
}

// Выбирает ограниченное восстановление после сбоя либо освобождает схему.
EELRecovery CELFlyingAttackBehavior::Recover(const SELBehaviorValidation& Failure)
{
	return EELRecovery::Release;
}
