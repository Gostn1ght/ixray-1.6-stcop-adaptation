#include "stdafx.h"
#include <cmath>
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Behaviors/CELFlyingCruiseBehavior.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELFlyingGeometry.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "Level.h"

// Подготавливает схему к запуску и сбрасывает состояние предыдущего выполнения.
void CELFlyingCruiseBehavior::Enter()
{
	CELFlyingBehaviorBase::Enter();
	OrbitCenter = Adapter.Monster().Position();
	OrbitAngle = float(Adapter.Monster().ID() % 32) * PI_MUL_2 / 32.f;
	OrbitCenter.x -= std::sin(OrbitAngle) * Settings.OrbitRadius;
	OrbitCenter.z -= std::cos(OrbitAngle) * Settings.OrbitRadius;
}

// Назначает интересующую точку и срок удержания внимания.
void CELFlyingCruiseBehavior::SetAttention(const Fvector& Position, bool IsEscape)
{
	if (!HasAttention || IsEscaping != IsEscape || AttentionPosition.distance_to_sqr(Position) > Settings.CollisionRadius * Settings.CollisionRadius)
	{
		Navigation.Cancel();
		HasPreparedRoute = false;
		RetryAfter = 0;
	}
	if (IsEscape && (!HasAttention || !IsEscaping))
	{
		Movement.RetainRoutePrefix(std::max(Settings.CollisionRadius * 2.f, Movement.GetSpeed() * Settings.InterruptLookahead));
	}
	AttentionPosition = Position;
	HasAttention = true;
	IsEscaping = IsEscape;
}

// Снимает временную интересующую точку свободного полёта.
void CELFlyingCruiseBehavior::ClearAttention()
{
	if (HasAttention)
	{
		Navigation.Cancel();
		HasPreparedRoute = false;
		RetryAfter = 0;
	}
	HasAttention = false;
	IsEscaping = false;
}

// Выбирает следующую точку маршрута с учётом текущей фазы поведения.
void CELFlyingCruiseBehavior::ChooseGoal()
{
	OrbitAngle += PI_DIV_2;
	const Fvector Origin = Movement.HasRoute() ? Movement.EndPosition() : Adapter.Monster().Position();
	GoalPosition = HasAttention ? AttentionPosition : OrbitCenter;
	GoalPosition.x += std::sin(OrbitAngle) * Settings.OrbitRadius;
	GoalPosition.z += std::cos(OrbitAngle) * Settings.OrbitRadius;
	float SurfaceHeight;
	if (CELFlyingGeometry::Surface(GoalPosition, SurfaceHeight))
	{
		GoalPosition.y = SurfaceHeight + Settings.Altitude;
	}
	const Fbox Bounds = CELFlyingGeometry::AirBounds(Settings.BoundaryMargin, Settings.CollisionRadius, Settings.CollisionCenterHeight);
	clamp(GoalPosition.x, Bounds.min.x, Bounds.max.x);
	clamp(GoalPosition.y, Bounds.min.y, Bounds.max.y);
	clamp(GoalPosition.z, Bounds.min.z, Bounds.max.z);
	GoalDirection.set(std::cos(OrbitAngle), 0.f, -std::sin(OrbitAngle));
	if (HasAttention && IsEscaping)
	{
		Fvector Departure;
		Departure.sub(Origin, AttentionPosition);
		Departure.y = 0.f;
		if (Departure.square_magnitude() < EPS_L)
		{
			Departure = Movement.Direction();
			Departure.y = 0.f;
		}
		Departure.normalize_safe();
		GoalPosition.mad(Origin, Departure, Settings.OrbitRadius);
		GoalPosition.y = std::max(GoalPosition.y, Origin.y);
		GoalDirection = Departure;
		clamp(GoalPosition.x, Bounds.min.x, Bounds.max.x);
		clamp(GoalPosition.z, Bounds.min.z, Bounds.max.z);
	}
}
