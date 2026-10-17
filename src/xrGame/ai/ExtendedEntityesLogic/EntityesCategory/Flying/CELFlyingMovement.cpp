#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/CELFlyingMovement.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedAnomalies.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "movement_manager.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELSharedFlyingObstacles.h"


namespace ExtendedEntityesFlightMovement
{
// Находит точку впереди по ломаной на заданной длине пути.
Fvector LookaheadEndpoint(const SELNavigationResult& Route, u32 Segment, const Fvector& StartPosition, float Distance, u32& LastPoint)
{
	Fvector EndPosition = StartPosition;
	float Remaining = Distance;
	Fvector Direction;
	Fvector Previous = StartPosition;
	float Length;
	u32 PointIndex;
	LastPoint = Segment;
	for (PointIndex = Segment; PointIndex < Route.Points.size() && Remaining > EPS_L; ++PointIndex)
	{
		Direction.sub(Route.Points[PointIndex], Previous);
		Length = Direction.magnitude();
		if (Length <= EPS_L)
		{
			continue;
		}
		Direction.div(Length);
		EndPosition.mad(Previous, Direction, std::min(Remaining, Length));
		Previous = EndPosition;
		Remaining -= Length;
		LastPoint = PointIndex;
	}
	return EndPosition;
}
// Оценивает отклонение кривого участка от прямого проверочного коридора.
float EnvelopeDeviation(const SELNavigationResult& Route, u32 Segment, u32 LastPoint, const Fvector& StartPosition, const Fvector& EndPosition)
{
	Fvector Direction;
	u32 PointIndex;
	// Include the curved centerline in the straight lookahead envelope.
	Direction.sub(EndPosition, StartPosition);
	const float LengthSquared = Direction.square_magnitude();
	Fvector Relative;
	Fvector Closest;
	float Fraction;
	float DeviationSquared = 0.f;
	for (PointIndex = Segment; PointIndex < LastPoint && LengthSquared > EPS_L; ++PointIndex)
	{
		Relative.sub(Route.Points[PointIndex], StartPosition);
		Fraction = Relative.dotproduct(Direction) / LengthSquared;
		clamp(Fraction, 0.f, 1.f);
		Closest.mad(StartPosition, Direction, Fraction);
		DeviationSquared = std::max(DeviationSquared, Closest.distance_to_sqr(Route.Points[PointIndex]));
	}
	return _sqrt(DeviationSquared);
}
} // namespace ExtendedEntityesFlightMovement

// Связывает движение по проверенной воздушной траектории с необходимыми службами и состоянием владельца.
CELFlyingMovement::CELFlyingMovement(IELAdapter& OwnerAdapter, const SELFlyingSettings& FlightSettings) : Adapter(OwnerAdapter), Settings(FlightSettings) {}
// Завершает движение по проверенной воздушной траектории и освобождает связанные ресурсы.
CELFlyingMovement::~CELFlyingMovement()
{
	Stop();
}
// Отклоняет наземное назначение: летуну нужен проверенный воздушный маршрут.
bool CELFlyingMovement::MoveTo(const Fvector& TargetPosition, u32 TargetVertex, bool IsRunning)
{
	return false;
}
// Задаёт допустимый индивидуальный множитель скорости.
void CELFlyingMovement::SetSpeedScale(float Scale)
{
	SpeedScale = _valid(Scale) ? Scale : 1.f;
	clamp(SpeedScale, .1f, 2.f);
}

// Останавливает движение и отменяет связанные проверки маршрута.
void CELFlyingMovement::Stop()
{
	CELSharedFlyingObstacles::Cancel(ActiveRoute.Identity);
	HasDynamicObstacle = false;
	IsBraking = false;
	ActiveRoute = SELNavigationResult();
	Velocity.set(0.f, 0.f, 0.f);
	Speed = 0.f;
}
// Сообщает, остались ли непройденные точки активного маршрута.
bool CELFlyingMovement::HasRoute() const
{
	return Segment < ActiveRoute.Points.size();
}
// Возвращает долю пройденных точек текущего маршрута.
float CELFlyingMovement::Progress() const
{
	return ActiveRoute.Points.empty() ? 0.f : float(Segment) / float(ActiveRoute.Points.size());
}
// Возвращает направление скорости либо текущую ось сущности при остановке.
Fvector CELFlyingMovement::Direction() const
{
	return Velocity.square_magnitude() > EPS_L ? Fvector(Velocity).normalize_safe() : Adapter.Monster().XFORM().k;
}
// Возвращает конечную точку активного маршрута.
const Fvector& CELFlyingMovement::EndPosition() const
{
	return ActiveRoute.Points.back();
}
// Возвращает конечную касательную активного маршрута.
const Fvector& CELFlyingMovement::EndDirection() const
{
	return FinalDirection;
}

// Принимает проверенную последовательность точек и заменяет предыдущий маршрут.
void CELFlyingMovement::SetRoute(SELNavigationResult Route)
{
	if (!Route.HasPath || Route.Points.size() < 2)
	{
		return;
	}
	CELSharedFlyingObstacles::Cancel(ActiveRoute.Identity);
	HasDynamicObstacle = false;
	IsBraking = false;
	FinalDirection.sub(Route.Points.back(), Route.Points[Route.Points.size() - 2]).normalize_safe();
	ActiveRoute = std::move(Route);
	Segment = 1;
}

// Обновляет движение по проверенной воздушной траектории и обрабатывает текущий результат выполнения.
void CELFlyingMovement::Update(float DeltaTime)
{
	if (!HasRoute() || DeltaTime <= 0.f)
	{
		Velocity.set(0.f, 0.f, 0.f);
		return;
	}
	float ClearDistance;
	if (CELSharedFlyingObstacles::IsBlocked(ActiveRoute.Identity, LastObstacle, ClearDistance))
	{
		ObstacleUntil = Device.dwTimeGlobal + Settings.ObstacleMemory;
		ClearDistance -= Settings.GroundClearance;
		if (ClearDistance <= EPS_L)
		{
			Stop();
			HasDynamicObstacle = true;
			return;
		}
		RetainRoutePrefix(ClearDistance);
		HasDynamicObstacle = true;
		IsBraking = true;
	}
	PublishObstacleProbe();
	clamp(DeltaTime, 0.f, .1f);
	CBaseMonster& OwnerMonster = Adapter.Monster();
	Fvector DirectionToPoint;
	DirectionToPoint.sub(ActiveRoute.Points[Segment], OwnerMonster.Position());
	DirectionToPoint.normalize_safe();
	const float Alignment = std::max(-1.f, std::min(1.f, OwnerMonster.XFORM().k.dotproduct(DirectionToPoint)));
	const float TurnRatio = 1.f - (1.f - Settings.TurningSpeedRatio) * std::min(1.f, (1.f - Alignment) * 2.f);
	const float VerticalRatio = DirectionToPoint.y > 0.f ? 1.f + DirectionToPoint.y * (Settings.ClimbSpeedRatio - 1.f) : 1.f - DirectionToPoint.y * (Settings.DiveSpeedRatio - 1.f);
	float TargetSpeed = Settings.Speed * SpeedScale * TurnRatio * VerticalRatio;
	if (IsLanding || IsBraking)
	{
		const float Remaining = OwnerMonster.Position().distance_to(ActiveRoute.Points.back());
		TargetSpeed = std::min(TargetSpeed, std::max(IsBraking ? 0.f : Settings.LandingSpeedMinimum, _sqrt(2.f * Settings.Acceleration * Remaining)));
	}
	Speed += std::min(Settings.Acceleration * DeltaTime, std::max(0.f, TargetSpeed - Speed));
	if (Speed > TargetSpeed)
	{
		Speed = std::max(TargetSpeed, Speed - Settings.Acceleration * DeltaTime);
	}
	float RemainingDistance = Speed * DeltaTime;
	float SegmentDistance;
	float TravelDistance;
	Fvector PreviousPosition = OwnerMonster.Position();
	Fvector NewPosition = PreviousPosition;
	while (RemainingDistance > EPS_L && HasRoute())
	{
		DirectionToPoint.sub(ActiveRoute.Points[Segment], NewPosition);
		SegmentDistance = DirectionToPoint.magnitude();
		if (SegmentDistance <= EPS_L)
		{
			++Segment;
			continue;
		}
		DirectionToPoint.div(SegmentDistance);
		TravelDistance = std::min(RemainingDistance, SegmentDistance);
		NewPosition.mad(DirectionToPoint, TravelDistance);
		RemainingDistance -= TravelDistance;
		if (TravelDistance >= SegmentDistance)
		{
			++Segment;
		}
	}
	if (CELSharedAnomalies::Crosses(OwnerMonster.ID(), PreviousPosition, NewPosition))
	{
		Stop();
		return;
	}
	Velocity.sub(NewPosition, PreviousPosition).div(DeltaTime);
	float Heading;
	float Pitch;
	float Bank;
	OwnerMonster.XFORM().getHPB(Heading, Pitch, Bank);
	float DesiredHeading;
	float DesiredPitch;
	DirectionToPoint.getHP(DesiredHeading, DesiredPitch);
	const float Turn = angle_normalize_signed(DesiredHeading - Heading);
	const float Response = 1.f - expf(-Settings.OrientationResponse * DeltaTime);
	// Orientation follows the certified trajectory quickly; bank eases independently.
	float HeadingStep = Turn * Response;
	clamp(HeadingStep, -Settings.TurnRate * DeltaTime, Settings.TurnRate * DeltaTime);
	Heading = angle_normalize_signed(Heading + HeadingStep);
	Pitch += (DesiredPitch - Pitch) * Response;
	Bank += (std::max(-Settings.MaximumBank, std::min(Settings.MaximumBank, -Turn)) - Bank) * Response;
	OwnerMonster.XFORM().setHPB(Heading, Pitch, Bank);
	OwnerMonster.Position() = NewPosition;
	OwnerMonster.movement().m_body.current.yaw = Heading;
	OwnerMonster.movement().m_body.current.pitch = Pitch;
	OwnerMonster.movement().m_body.target = OwnerMonster.movement().m_body.current;
	OwnerMonster.spatial_move();
	if (!HasRoute())
	{
		CELSharedFlyingObstacles::Cancel(ActiveRoute.Identity);
	}
}

// Оставляет безопасный участок старого маршрута без сброса текущей скорости.
void CELFlyingMovement::RetainRoutePrefix(float Distance)
{
	if (!HasRoute() || !_valid(Distance) || Distance <= EPS_L)
	{
		return;
	}
	SELNavigationResult Prefix;
	Prefix.Identity = ActiveRoute.Identity;
	Prefix.HasPath = true;
	Prefix.Points.push_back(Adapter.Monster().Position());
	Fvector Previous = Prefix.Points.back();
	Fvector Direction;
	Fvector Endpoint;
	float SegmentLength;
	u32 PointIndex;
	for (PointIndex = Segment; PointIndex < ActiveRoute.Points.size() && Distance > EPS_L; ++PointIndex)
	{
		Direction.sub(ActiveRoute.Points[PointIndex], Previous);
		SegmentLength = Direction.magnitude();
		if (SegmentLength <= EPS_L)
		{
			continue;
		}
		Direction.div(SegmentLength);
		Endpoint.mad(Previous, Direction, std::min(Distance, SegmentLength));
		Prefix.Points.push_back(Endpoint);
		Previous = Endpoint;
		Distance -= SegmentLength;
	}
	if (Prefix.Points.size() > 1)
	{
		SetRoute(std::move(Prefix));
	}
}

// Добавляет ещё актуальные препятствия в снимок следующего обхода.
void CELFlyingMovement::AppendObstacles(xr_vector<SELNavigationObstacle>& Obstacles) const
{
	if (s32(Device.dwTimeGlobal - ObstacleUntil) < 0)
	{
		Obstacles.push_back(LastObstacle);
	}
}
// Публикует коридор впереди с запасом на торможение и кривизну.
void CELFlyingMovement::PublishObstacleProbe()
{
	SELFlyingCorridor Corridor;
	Corridor.Identity = ActiveRoute.Identity;
	Corridor.Start = Adapter.Monster().Position();
	Corridor.End = Corridor.Start;
	Corridor.CenterHeight = Settings.CollisionCenterHeight;
	Corridor.Radius = Settings.CollisionRadius;
	Corridor.ContactTarget = ContactTarget;
	Corridor.IsLanding = IsLanding;
	u32 LastPoint;
	Corridor.End = ExtendedEntityesFlightMovement::LookaheadEndpoint(ActiveRoute, Segment, Corridor.Start, std::max(Settings.CollisionRadius * 2.f, (Speed * Speed / (2.f * Settings.Acceleration)) + Settings.Speed * Settings.ObstacleLookahead), LastPoint);
	Corridor.Radius += ExtendedEntityesFlightMovement::EnvelopeDeviation(ActiveRoute, Segment, LastPoint, Corridor.Start, Corridor.End);
	CELSharedFlyingObstacles::Publish(Corridor);
}
