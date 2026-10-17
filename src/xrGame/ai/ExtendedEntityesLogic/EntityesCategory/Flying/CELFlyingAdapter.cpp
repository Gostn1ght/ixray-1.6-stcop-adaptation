#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/CELFlyingAdapter.h"
#include "ai/monsters/basemonster/base_monster.h"
#include "ai/monsters/control_path_builder_base.h"
#include "Level.h"

// Проверяет допустимость назначения с учётом ограничений категории.
bool CELFlyingAdapter::CanReach(const Fvector& TargetPosition, u32 TargetVertex) const
{
	return _valid(TargetPosition) && Level().ObjectSpace.GetBoundingVolume().contains(TargetPosition);
}

// Отключает штатное наземное построение пути у воздушной сущности.
void CELFlyingAdapter::ReleaseControl()
{
	// The native flying entity has no ground FSM to restart or character to move.
	OwnerMonster.path().disable_path();
}
