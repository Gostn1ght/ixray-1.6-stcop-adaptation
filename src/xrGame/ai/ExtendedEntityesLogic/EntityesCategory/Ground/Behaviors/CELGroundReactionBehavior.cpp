#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/Debug/Debug.h"
#include <cmath>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Ground/Behaviors/CELGroundReactionBehavior.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "ai_space.h"
#include "level_graph.h"

// Настраивает условия следующего запуска схемы.
void CELGroundReactionBehavior::Configure(const Fvector& SoundPosition, u32 ReactionUntil, bool ShouldFlee)
{
	if (IsFleeing != ShouldFlee || SourcePosition.distance_to_sqr(SoundPosition) > 4.f)
	{
		HasPoint = false;
	}
	SourcePosition = SoundPosition;
	Until = ReactionUntil;
	IsFleeing = ShouldFlee;
}

// Подготавливает схему к запуску и сбрасывает состояние предыдущего выполнения.
void CELGroundReactionBehavior::Enter()
{
	CELGroundBehaviorBase::Enter();
	HasPoint = false;
	NextPoint = 0;
}

// Выбирает достижимую точку для реакции на стимул.
bool CELGroundReactionBehavior::SelectPoint()
{
	Fvector Away;
	Fvector Candidate;
	float Heading;
	float Pitch;
	float Angle;
	u32 CandidateVertex;
	u32 Attempt;
	Away.sub(Adapter.Monster().Position(), SourcePosition);
	Away.y = 0.f;
	if (Away.square_magnitude() < EPS_L)
	{
		Away = Adapter.Monster().XFORM().k;
		Away.y = 0.f;
	}
	Away.normalize_safe();
	Away.getHP(Heading, Pitch);
	for (Attempt = 0; Attempt < 8; ++Attempt)
	{
		if (IsFleeing)
		{
			Angle = Heading + float(Attempt % 2 ? s32(Attempt) : -s32(Attempt)) * PI_DIV_6;
			Candidate = Adapter.Monster().Position();
			Candidate.x -= std::sin(Angle) * 12.f;
			Candidate.z += std::cos(Angle) * 12.f;
		}
		else
		{
			Candidate = SourcePosition;
		}
		CandidateVertex = ai().level_graph().vertex_id(Candidate);
		if (ai().level_graph().valid_vertex_id(CandidateVertex))
		{
			Candidate.y = ai().level_graph().vertex_plane_y(CandidateVertex, Candidate.x, Candidate.z);
		}
		if (Adapter.CanReach(Candidate, CandidateVertex))
		{
			ReactionPosition = Candidate;
			ReactionVertex = CandidateVertex;
			Navigation.Reset();
			Route = SELNavigationResult();
			RouteCursor = 0;
			return true;
		}
		if (!IsFleeing)
		{
			break;
		}
	}
	return false;
}

// Обновляет реакцию на опасную или интересующую точку и обрабатывает текущий результат выполнения.
void CELGroundReactionBehavior::Update(float DeltaTime)
{
	if (s32(Device.dwTimeGlobal - Until) >= 0)
	{
		Movement.Stop();
		Validation = {EELBehaviorState::Completed, "Реакция на шум завершена"};
		return;
	}
	if (!HasPoint && s32(Device.dwTimeGlobal - NextPoint) >= 0)
	{
		HasPoint = SelectPoint();
		NextPoint = Device.dwTimeGlobal + 1500;
	}
	if (!HasPoint)
	{
		Validation = {EELBehaviorState::Blocked, "Нет доступной точки реакции"};
		return;
	}
	if (Adapter.Monster().Position().distance_to_sqr(ReactionPosition) < 2.25f)
	{
		Movement.Stop();
		HasPoint = !IsFleeing;
		Validation = {EELBehaviorState::Running, IsFleeing ? "Бегство от шума" : "Осмотр источника шума"};
		ProgressTime = Device.dwTimeGlobal;
		return;
	}
	if (CELSharedDebug::Instance().IsDrawEnabled())
	{
		SELDebugEvent Event;
		Event.Kind = EELDebugEvent::Search;
		Event.OwnerIdentifier = Adapter.Monster().ID();
		Event.Start = Adapter.Monster().Position();
		Event.Finish = ReactionPosition;
		Event.Radius = .4f;
		Event.Label = IsFleeing ? "Уход от шума" : "Исследование шума";
		CELSharedDebug::Instance().PublishEvent(std::move(Event));
	}
	Validation = {EELBehaviorState::Running, IsFleeing ? "Бегство от шума" : "Подход к источнику шума"};
	if (!Move(ReactionPosition, ReactionVertex, IsFleeing))
	{
		HasPoint = false;
		Validation = {EELBehaviorState::Blocked, "Путь к источнику шума перекрыт"};
	}
}
