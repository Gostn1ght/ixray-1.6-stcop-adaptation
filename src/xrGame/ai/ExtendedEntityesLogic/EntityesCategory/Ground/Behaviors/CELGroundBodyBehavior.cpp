#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Ground/Behaviors/CELGroundBodyBehavior.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedDeaths.h"
#include "ai/ExtendedEntityesLogic/Interfaces/IELAdapter.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "ai/monsters/control_direction_base.h"
#include "ai_monster_space.h"

// Связывает исследование известного тела с необходимыми службами и состоянием владельца.
CELGroundBodyBehavior::CELGroundBodyBehavior(IELAdapter& OwnerAdapter, const SELPerceptionProfile& OwnerProfile) : CELGroundBehaviorBase(OwnerAdapter, OwnerProfile), Inspection(OwnerAdapter.Monster()) {}

// Подготавливает схему к запуску и сбрасывает состояние предыдущего выполнения.
void CELGroundBodyBehavior::Enter()
{
	CELGroundBehaviorBase::Enter();
	InspectUntil = 0;
	HasReachedBody = false;
}

// Обновляет исследование известного тела и обрабатывает текущий результат выполнения.
void CELGroundBodyBehavior::Update(float DeltaTime)
{
	if (Device.dwTimeGlobal - StartedAt > Profile.SearchMax + 5000)
	{
		Validation = {EELBehaviorState::NoProgress, "Body investigation deadline"};
		return;
	}
	if (Adapter.Monster().Position().distance_to_sqr(Observation.Position) < 6.25f)
	{
		if (!HasReachedBody)
		{
			HasReachedBody = true;
			InspectUntil = Device.dwTimeGlobal + Random.randI(3000u, 6001u);
		}
		Movement.Stop();
		Adapter.Monster().dir().face_target(Observation.Position);
		Adapter.Monster().set_action(ACT_LOOK_AROUND);
		Inspection.Update(Profile.CanInspect);
		HasMoveCommand = false;
		ProgressTime = Device.dwTimeGlobal;
		if (s32(Device.dwTimeGlobal - InspectUntil) >= 0)
		{
			CELSharedDeaths::Release(Adapter.Monster().ID(), true);
			Validation = {EELBehaviorState::Completed, "Body investigated"};
		}
		else
		{
			Validation = {EELBehaviorState::Running, "Inspecting body"};
		}
		return;
	}
	Validation = {EELBehaviorState::Running, "Approaching known body"};
	if (!Move(Observation.Position, Observation.VertexIdentifier, false))
	{
		Validation = {EELBehaviorState::Blocked, "Body route blocked"};
	}
}

// Завершает активную схему и освобождает временные заявки.
void CELGroundBodyBehavior::Exit()
{
	Inspection.Clear();
	CELSharedDeaths::Release(Adapter.Monster().ID(), false);
	CELGroundBehaviorBase::Exit();
}
