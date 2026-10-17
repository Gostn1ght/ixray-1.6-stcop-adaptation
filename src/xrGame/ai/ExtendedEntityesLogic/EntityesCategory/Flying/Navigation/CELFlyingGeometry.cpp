#include "stdafx.h"
#include <cmath>
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELFlyingGeometry.h"
#include "ai/ExtendedEntityesLogic/EntryPoint.h"
#include "ai/ExtendedEntityesLogic/CELSharedSettings.h"
#include "ai/ExtendedEntityesLogic/Navigation/CELSharedNavigationScheduler.h"
#include "Level.h"
#include "ai_space.h"
#include "level_graph.h"
#include "entity_alive.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/CELFlyingEntityBase.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELSharedFlyingObstacles.h"
#include "../xrEngine/GameMtlLib.h"

namespace ExtendedEntityesGeometry
{

struct SRayQuery
{
	collide::rq_result& Nearest;
	bool IsSupport = false;
	const SELFlyingCorridor* Corridor = nullptr;
};
// Отбирает ближайшее значимое препятствие с учётом материалов и режима коридора.
bool InspectSolid(collide::rq_result& Hit, LPVOID Parameter)
{
	SRayQuery& Query = *static_cast<SRayQuery*>(Parameter);

	if (Hit.O && Query.Corridor)
	{
		if (Hit.O->ID() == Query.Corridor->ContactTarget)
		{
			return true;
		}
		const CELFlyingEntityBase* Flyer = smart_cast<CELFlyingEntityBase*>(Hit.O);
		if (!Query.Corridor->IsLanding && Flyer && Flyer->g_Alive() && !Flyer->IsFlightGrounded())
		{
			return true;
		}
		Fvector Center;
		Hit.O->Center(Center);
		Fvector Start = Query.Corridor->Start;
		Fvector End = Query.Corridor->End;
		Start.y += Query.Corridor->CenterHeight;
		End.y += Query.Corridor->CenterHeight;
		const float StartDistance = Start.distance_to_sqr(Center);
		Fvector Away;
		Fvector Departure;
		Away.sub(Start, Center);
		Departure.sub(End, Start);
		if (StartDistance < _sqr(Query.Corridor->Radius + Hit.O->Radius()) && End.distance_to_sqr(Center) > StartDistance && Away.dotproduct(Departure) >= 0.f)
		{
			return true;
		}
	}
	if (!Hit.O)
	{
		const SGameMtl* Material = GMLib.GetMaterialByIdx(Level().ObjectSpace.GetStaticTris()[Hit.element].material);
		if (Material && Material->Flags.is(SGameMtl::flPassable) && !(Query.IsSupport && Material->Flags.is(SGameMtl::flLiquid)))
		{
			return true;
		}
	}
	if (Query.Nearest.element < 0 || Hit.range < Query.Nearest.range)
	{
		Query.Nearest = Hit;
	}
	return true;
}
// Выполняет штатный лучевой запрос с отбором значимых столкновений.
bool PickSolid(const Fvector& From, const Fvector& Direction, float Distance, collide::rq_target Target, collide::rq_result& Hit, CObject* Ignore, bool IsSupport = false, const SELFlyingCorridor* Corridor = nullptr)
{
	Hit.set(nullptr, Distance, -1);
	collide::rq_results Results;
	collide::ray_defs Ray(From, Direction, Distance, CDB::OPT_FULL_TEST, Target);
	SRayQuery Query{Hit, IsSupport, Corridor};
	Level().ObjectSpace.RayQuery(Results, Ray, InspectSolid, &Query, nullptr, Ignore);
	return Hit.element >= 0;
}
struct SValidation
{
	SELNavigationResult Route;
	float Radius = .35f;
	float CenterHeight = .4f;
	u32 Segment = 1;
	u32 Probe = 0;
	EELGeometryResult State = EELGeometryResult::Pending;
};
const Fvector Offsets[] = {{0, 0, 0}, {1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, 1}, {-1, -1, -1}};
} // namespace ExtendedEntityesGeometry
struct CELFlyingGeometry::SState
{
	xr_vector<ExtendedEntityesGeometry::SValidation> Validations;
	u32 NextValidation = 0;
};
// Возвращает общее хранилище состояния очереди.
CELFlyingGeometry::SState& CELFlyingGeometry::GetState()
{
	static SState Runtime;
	return Runtime;
}


// Возвращает границы воздушных назначений с запасом под размеры тела.
Fbox CELFlyingGeometry::AirBounds(float HorizontalMargin, float Radius, float CenterHeight)
{
	Fbox Bounds = Level().ObjectSpace.GetBoundingVolume();
	HorizontalMargin = std::max(HorizontalMargin, Radius);
	const float MarginX = std::min(HorizontalMargin, (Bounds.max.x - Bounds.min.x) * .25f);
	const float MarginZ = std::min(HorizontalMargin, (Bounds.max.z - Bounds.min.z) * .25f);
	const float HeightLimit = (Bounds.max.y - Bounds.min.y) * .25f;
	const float LowerMargin = std::min(std::max(0.f, Radius - CenterHeight), HeightLimit);
	const float UpperMargin = std::min(Radius + CenterHeight, HeightLimit);
	Bounds.min.x += MarginX;
	Bounds.max.x -= MarginX;
	Bounds.min.y += LowerMargin;
	Bounds.max.y -= UpperMargin;
	Bounds.min.z += MarginZ;
	Bounds.max.z -= MarginZ;
	return Bounds;
}

// Помещает заявку в очередь с проверкой допустимости и доступного места.
bool CELFlyingGeometry::Submit(SELNavigationResult Route, float Radius, float CenterHeight)
{
	using namespace ExtendedEntityesGeometry;
	if (GetState().Validations.size() >= CELSharedSettings::Work().NavigationTaskLimit || Route.Points.size() < 2)
	{
		return false;
	}
	SValidation Validation;
	Validation.Route = std::move(Route);
	Validation.Radius = Radius;
	Validation.CenterHeight = CenterHeight;
	GetState().Validations.push_back(std::move(Validation));
	return true;
}

// Возвращает состояние заявки и передаёт готовые данные вызывающему коду.
EELGeometryResult CELFlyingGeometry::Take(const SELNavigationIdentity& Identity, SELNavigationResult& Route)
{
	using namespace ExtendedEntityesGeometry;
	xr_vector<SValidation>::iterator ValidationIterator;
	EELGeometryResult State;
	for (ValidationIterator = GetState().Validations.begin(); ValidationIterator != GetState().Validations.end(); ++ValidationIterator)
	{
		if (ValidationIterator->Route.Identity == Identity)
		{
			if (ValidationIterator->State == EELGeometryResult::Pending)
			{
				return EELGeometryResult::Pending;
			}
			State = ValidationIterator->State;
			Route = std::move(ValidationIterator->Route);
			GetState().Validations.erase(ValidationIterator);
			return State;
		}
	}
	return EELGeometryResult::Blocked;
}

// Отменяет принадлежащую владельцу заявку и очищает промежуточный результат.
void CELFlyingGeometry::Cancel(const SELNavigationIdentity& Identity)
{
	using namespace ExtendedEntityesGeometry;
	xr_vector<SValidation>::iterator ValidationIterator = GetState().Validations.begin();
	while (ValidationIterator != GetState().Validations.end())
	{
		if (ValidationIterator->Route.Identity == Identity)
		{
			ValidationIterator = GetState().Validations.erase(ValidationIterator);
		}
		else
		{
			++ValidationIterator;
		}
	}
}

// Порционно проверяет маршруты лучами, сохраняя состояние незавершённых проверок.
void CELFlyingGeometry::BeginFrame()
{
	using namespace ExtendedEntityesGeometry;
	if (!g_pGameLevel)
	{
		return;
	}
	u32 Skipped = 0;
	SValidation* Validation;
	Fvector StartPosition;
	Fvector EndPosition;
	Fvector Direction;
	float Distance;
	EELGeometryResult FloorState;
	collide::rq_result Hit;
	const Fbox& Bounds = Level().ObjectSpace.GetBoundingVolume();
	while (!GetState().Validations.empty() && Skipped < GetState().Validations.size())
	{
		GetState().NextValidation %= u32(GetState().Validations.size());
		Validation = &GetState().Validations[GetState().NextValidation++];
		if (Validation->State != EELGeometryResult::Pending)
		{
			++Skipped;
			continue;
		}
		if (!ExtendedEntityes::Navigation().TryGeometryRays(Validation->Probe == 8 ? 2u : 1u))
		{
			break;
		}
		Skipped = 0;
		StartPosition.mad(Validation->Route.Points[Validation->Segment - 1], Offsets[Validation->Probe], Validation->Radius);
		EndPosition.mad(Validation->Route.Points[Validation->Segment], Offsets[Validation->Probe], Validation->Radius);
		StartPosition.y += Validation->CenterHeight;
		EndPosition.y += Validation->CenterHeight;
		Direction.sub(EndPosition, StartPosition);
		Distance = Direction.magnitude();
		if (!Bounds.contains(StartPosition) || !Bounds.contains(EndPosition))
		{
			Validation->State = EELGeometryResult::Blocked;
			continue;
		}
		if (Distance > EPS_L)
		{
			Direction.div(Distance);
			if (ExtendedEntityesGeometry::PickSolid(StartPosition, Direction, Distance, collide::rqtStatic, Hit, nullptr))
			{
				Validation->State = EELGeometryResult::Blocked;
				continue;
			}
		}
		if (++Validation->Probe == 9)
		{
			FloorState = CheckFloor(Validation->Route.Points[Validation->Segment], Validation->Radius, Validation->CenterHeight);
			if (FloorState == EELGeometryResult::Pending)
			{
				Validation->Probe = 8;
				break;
			}
			if (FloorState == EELGeometryResult::Blocked)
			{
				Validation->State = EELGeometryResult::Blocked;
				continue;
			}
			Validation->Probe = 0;
			if (++Validation->Segment == Validation->Route.Points.size())
			{
				Validation->State = EELGeometryResult::Clear;
			}
		}
	}
}

// Определяет высоту поверхности под заданной точкой.
bool CELFlyingGeometry::Surface(const Fvector& Position, float& Height, bool HasReservedRay)
{
	if (!g_pGameLevel)
	{
		return false;
	}
	if (!HasReservedRay && !ExtendedEntityes::Navigation().TryGeometryRays(1))
	{
		return false;
	}
	Fvector StartPosition = Position;
	StartPosition.y = Level().ObjectSpace.GetBoundingVolume().max.y + 1.f;
	const Fvector Direction = {0.f, -1.f, 0.f};
	collide::rq_result Hit;
	if (!ExtendedEntityesGeometry::PickSolid(StartPosition, Direction, StartPosition.y - Level().ObjectSpace.GetBoundingVolume().min.y + 2.f, collide::rqtStatic, Hit, nullptr, true))
	{
		return false;
	}
	Height = StartPosition.y - Hit.range;
	return true;
}

// Проверяет достаточный зазор между телом и поверхностью.
EELGeometryResult CELFlyingGeometry::CheckFloor(const Fvector& Position, float Radius, float CenterHeight)
{
	float HighestSurface;
	if (!Surface(Position, HighestSurface, true) || Position.y + CenterHeight - Radius >= HighestSurface - .02f)
	{
		return EELGeometryResult::Clear;
	}
	// A roof above the route is not terrain below it. Require a real native floor
	// and a valid graph anchor before allowing a covered/underground air corridor.
	if (!ai().get_level_graph() || !ai().level_graph().valid_vertex_position(Position))
	{
		return EELGeometryResult::Blocked;
	}
	const u32 Vertex = ai().level_graph().vertex_id(Position);
	if (!ai().level_graph().valid_vertex_id(Vertex))
	{
		return EELGeometryResult::Blocked;
	}
	const float FloorHeight = ai().level_graph().vertex_plane_y(Vertex, Position.x, Position.z);
	if (Position.y + CenterHeight - Radius < FloorHeight - .02f)
	{
		return EELGeometryResult::Blocked;
	}
	if (!ExtendedEntityes::Navigation().TryGeometryRays(1))
	{
		return EELGeometryResult::Pending;
	}
	Fvector From = Position;
	From.y += CenterHeight;
	const Fvector Down = {0.f, -1.f, 0.f};
	collide::rq_result Hit;
	if (!ExtendedEntityesGeometry::PickSolid(From, Down, std::max(Radius, From.y - FloorHeight + Radius), collide::rqtStatic, Hit, nullptr, true))
	{
		return EELGeometryResult::Blocked;
	}
	return Hit.range + .02f >= Radius ? EELGeometryResult::Clear : EELGeometryResult::Blocked;
}

// Проверяет наличие перекрытия над предполагаемым укрытием.
bool CELFlyingGeometry::HasCover(const Fvector& Point, float Height)
{
	if (!g_pGameLevel || !ExtendedEntityes::Navigation().TryGeometryRays(1))
	{
		return false;
	}
	const Fvector Up = {0.f, 1.f, 0.f};
	collide::rq_result Hit;
	return ExtendedEntityesGeometry::PickSolid(Point, Up, Height, collide::rqtStatic, Hit, nullptr);
}

// Проверяет опору, возвращая положение контакта и нормаль поверхности.
EELGeometryResult CELFlyingGeometry::QuerySupport(const Fvector& From, float Distance, Fvector& Contact, Fvector& Normal, CObject* Ignore, bool HasReservedRay)
{
	if (!g_pGameLevel || !_valid(From) || !_valid(Distance) || Distance <= EPS_L)
	{
		return EELGeometryResult::Blocked;
	}
	if (!HasReservedRay && !ExtendedEntityes::Navigation().TryGeometryRays(1))
	{
		return EELGeometryResult::Pending;
	}
	const Fvector Down = {0.f, -1.f, 0.f};
	collide::rq_result Hit;
	if (!ExtendedEntityesGeometry::PickSolid(From, Down, Distance, collide::rqtStatic, Hit, Ignore, true))
	{
		return EELGeometryResult::Blocked;
	}
	Contact.mad(From, Down, Hit.range);
	const CDB::TRI& Triangle = Level().ObjectSpace.GetStaticTris()[Hit.element];
	const SGameMtl* Material = GMLib.GetMaterialByIdx(Triangle.material);
	if (Material && Material->Flags.is(SGameMtl::flLiquid))
	{
		return EELGeometryResult::Blocked;
	}
	const xr_vector<Fvector>& Vertices = Level().ObjectSpace.GetStaticVerts();
	Normal.mknormal(Vertices[Triangle.verts[0]], Vertices[Triangle.verts[1]], Vertices[Triangle.verts[2]]);
	return Normal.y > EPS_L ? EELGeometryResult::Clear : EELGeometryResult::Blocked;
}

// Возвращает только подтверждённую опору при доступном бюджете.
bool CELFlyingGeometry::Support(const Fvector& From, float Distance, Fvector& Contact, Fvector& Normal, CObject* Ignore, bool HasReservedRay)
{
	return QuerySupport(From, Distance, Contact, Normal, Ignore, HasReservedRay) == EELGeometryResult::Clear;
}

// Проверяет отрезок на столкновение, отличая ожидание квоты от препятствия.
EELGeometryResult CELFlyingGeometry::QuerySegment(const Fvector& Start, const Fvector& End, CObject* Ignore, bool HasReservedRay)
{
	if (!g_pGameLevel || !_valid(Start) || !_valid(End))
	{
		return EELGeometryResult::Blocked;
	}
	Fvector Direction;
	Direction.sub(End, Start);
	const float Distance = Direction.magnitude();
	if (Distance < EPS_L)
	{
		return EELGeometryResult::Clear;
	}
	if (!HasReservedRay && !ExtendedEntityes::Navigation().TryGeometryRays(1))
	{
		return EELGeometryResult::Pending;
	}
	Direction.div(Distance);
	collide::rq_result Hit;
	return ExtendedEntityesGeometry::PickSolid(Start, Direction, Distance, collide::rqtBoth, Hit, Ignore) ? EELGeometryResult::Blocked : EELGeometryResult::Clear;
}

// Сообщает, подтверждена ли проходимость заданного отрезка.
bool CELFlyingGeometry::ClearSegment(const Fvector& Start, const Fvector& End, CObject* Ignore)
{
	return QuerySegment(Start, End, Ignore) == EELGeometryResult::Clear;
}

// Сбрасывает накопленные данные и незавершённую работу подсистемы.
void CELFlyingGeometry::Clear()
{
	GetState().Validations.clear();
	GetState().NextValidation = 0;
}

// Возвращает число заявок, ожидающих обработки или получения результата.
u32 CELFlyingGeometry::OutstandingRequests()
{
	return u32(GetState().Validations.size());
}


// Проверяет размещение корпуса лучами вдоль трёх осей.
EELGeometryResult CELFlyingGeometry::QueryBody(const Fvector& Position, float Radius, float CenterHeight, CObject* Ignore, bool HasReservedRays)
{
	if (!HasReservedRays && !ExtendedEntityes::Navigation().TryGeometryRays(3))
	{
		return EELGeometryResult::Pending;
	}
	const Fvector Axes[] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};
	Fvector Center = Position;
	Center.y += CenterHeight;
	Fvector Start;
	Fvector End;
	u32 ProbeIndex;
	for (ProbeIndex = 0; ProbeIndex < std::size(Axes); ++ProbeIndex)
	{
		Start.mad(Center, Axes[ProbeIndex], -Radius);
		End.mad(Center, Axes[ProbeIndex], Radius);
		if (QuerySegment(Start, End, Ignore, true) != EELGeometryResult::Clear)
		{
			return EELGeometryResult::Blocked;
		}
	}
	return EELGeometryResult::Clear;
}
// Проверяет площадку по опорам, уклону и свободному месту для тела.
EELGeometryResult CELFlyingGeometry::QueryLanding(const Fvector& From, float Distance, float Radius, float CenterHeight, float GroundOffset, float MinimumNormalY, float HeightTolerance, Fvector& RootPosition, CObject* Ignore)
{
	if (!ExtendedEntityes::Navigation().TryGeometryRays(8))
	{
		return EELGeometryResult::Pending;
	}
	const Fvector Footprint[] = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}};
	Fvector Probe;
	Fvector Contact;
	Fvector Normal;
	float SupportHeight = 0.f;
	u32 ProbeIndex;
	for (ProbeIndex = 0; ProbeIndex < std::size(Footprint); ++ProbeIndex)
	{
		Probe.mad(From, Footprint[ProbeIndex], Radius);
		if (QuerySupport(Probe, Distance, Contact, Normal, Ignore, true) != EELGeometryResult::Clear || Normal.y < MinimumNormalY)
		{
			return EELGeometryResult::Blocked;
		}
		if (ProbeIndex == 0)
		{
			SupportHeight = Contact.y;
		}
		else if (std::abs(Contact.y - SupportHeight) > HeightTolerance)
		{
			return EELGeometryResult::Blocked;
		}
	}
	RootPosition.set(From.x, SupportHeight + GroundOffset, From.z);
	return QueryBody(RootPosition, Radius, CenterHeight, Ignore, true);
}

// Проверяет движущиеся препятствия впереди и возвращает свободную дистанцию.
EELGeometryResult CELFlyingGeometry::QueryDynamicCorridor(const SELFlyingCorridor& Corridor, SELNavigationObstacle& Obstacle, float& ClearDistance)
{
	if (!g_pGameLevel || !ExtendedEntityes::Navigation().TryGeometryRays(9))
	{
		return EELGeometryResult::Pending;
	}
	CEntityAlive* Owner = ExtendedEntityes::FindEntity(Corridor.Identity.OwnerIdentifier);
	if (!Owner)
	{
		return EELGeometryResult::Blocked;
	}
	Fvector Direction;
	Direction.sub(Corridor.End, Corridor.Start);
	const float Distance = Direction.magnitude();
	if (Distance <= EPS_L)
	{
		return EELGeometryResult::Clear;
	}
	Direction.div(Distance);
	ClearDistance = Distance;
	bool HasObstacle = false;
	Fvector Start;
	collide::rq_result Hit;
	u32 ProbeIndex;
	for (ProbeIndex = 0; ProbeIndex < std::size(ExtendedEntityesGeometry::Offsets); ++ProbeIndex)
	{
		Start.mad(Corridor.Start, ExtendedEntityesGeometry::Offsets[ProbeIndex], Corridor.Radius);
		Start.y += Corridor.CenterHeight;
		if (ExtendedEntityesGeometry::PickSolid(Start, Direction, Distance, collide::rqtObject, Hit, Owner, false, &Corridor) && Hit.O && Hit.range <= ClearDistance)
		{
			Hit.O->Center(Obstacle.Position);
			Obstacle.Position.y -= Corridor.CenterHeight;
			Obstacle.Radius = Hit.O->Radius() + Corridor.Radius;
			ClearDistance = Hit.range;
			HasObstacle = true;
		}
	}
	return HasObstacle ? EELGeometryResult::Blocked : EELGeometryResult::Clear;
}
