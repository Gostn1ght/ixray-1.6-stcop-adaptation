#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Ground/Entityes/Aggressive/Behaviors/CELGroundAttackBehavior.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/ExtendedEntityesLogic/EntryPoint.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "ai_object_location.h"
#include "ai_space.h"
#include "level_graph.h"

// Подготавливает схему к запуску и сбрасывает состояние предыдущего выполнения.
void CELGroundAttackBehavior::Enter()
{
	CELGroundBehaviorBase::Enter();
	Approach.Reset();
	DeadlineTarget = u16(-1);
}

// Обновляет наземный подход к противнику и обрабатывает текущий результат выполнения.
void CELGroundAttackBehavior::Update(float DeltaTime)
{
	CEntityAlive* TargetEntity = ExtendedEntityes::FindEntity(Observation.TargetIdentifier);
	if (!TargetEntity || !Adapter.IsEnemy(*TargetEntity))
	{
		Validation = {EELBehaviorState::InvalidTarget, "Lost target"};
		return;
	}
	Fvector GoalPosition = Observation.Position;
	u32 GoalVertex = Observation.VertexIdentifier;
	const bool HasVision = Adapter.CanSee(*TargetEntity);
	if (HasVision || Profile.CanTrackLive || s32(Device.dwTimeGlobal - Observation.VisualGraceUntil) < 0)
	{
		GoalPosition = TargetEntity->Position();
		GoalVertex = TargetEntity->ai_location().level_vertex_id();
	}
	Validation = {EELBehaviorState::Running, HasVision ? "Approach visible" : "Last known point"};
	UpdateDeadline();
	if (s32(Device.dwTimeGlobal - ApproachDeadline) >= 0)
	{
		Validation = {EELBehaviorState::Completed, "Approach deadline"};
		Movement.Stop();
		return;
	}
	Approach.Select(Adapter, Profile, GoalPosition, HasVision);
	if (!Navigation.IsPending() && RouteGoal.distance_to_sqr(Approach.Position()) > 4.f && (!Route.HasPath || Adapter.Monster().Position().distance_to_sqr(RouteGoal) < 4.f))
	{
		Navigation.Reset();
		Route = SELNavigationResult();
		RouteCursor = 0;
		RouteGoal = Approach.Position();
	}
	Validation = {EELBehaviorState::Running, Approach.Description()};
	if (!Move(Approach.Position(), Approach.Vertex(), Approach.Runs()))
	{
		Validation = {EELBehaviorState::Blocked, "Unreachable point"};
	}
}

// Оценивает допустимое время движения по длине подготовленного маршрута.
void CELGroundAttackBehavior::UpdateDeadline()
{
	if (DeadlineTarget != Observation.TargetIdentifier)
	{
		DeadlineTarget = Observation.TargetIdentifier;
		ApproachDeadline = StartedAt + Profile.ApproachMax;
		HasRouteEstimate = false;
	}
	if (!Route.HasPath || HasRouteEstimate)
	{
		return;
	}
	float RouteLength = 0.f;
	u32 PointIndex;
	for (PointIndex = 1; PointIndex < Route.Points.size(); ++PointIndex)
	{
		RouteLength += Route.Points[PointIndex - 1].distance_to(Route.Points[PointIndex]);
	}
	float Duration = RouteLength / Profile.ApproachSpeed * 2000.f;
	clamp(Duration, float(Profile.ApproachMin), float(Profile.ApproachMax));
	const u32 Remaining = Profile.ApproachMax - std::min(Profile.ApproachMax, Device.dwTimeGlobal - StartedAt);
	ApproachDeadline = Device.dwTimeGlobal + std::min(Remaining, u32(Duration));
	HasRouteEstimate = true;
}
