#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/Debug/DebugDrawWorld.h"
#include "HUDManager.h"

// Добавляет маршруты, цели и индикаторы атаки в очередь HUD().world_prims.
void CELSharedDebugDrawWorld::Draw(const xr_vector<SELDebugRecord>& Records)
{
	xr_vector<SELDebugRecord>::const_iterator RecordIterator;
	u32 Color;
	string256 Label;
	for (RecordIterator = Records.begin(); RecordIterator != Records.end(); ++RecordIterator)
	{
		Color = RecordIterator->IsAttacking ? color_rgba(255, 30, 30, 180) : color_rgba(255, 220, 40, 160);
		HUD().world_prims.append_sphere(RecordIterator->Position, RecordIterator->Radius, Color);
		HUD().world_prims.append_line(RecordIterator->Position, RecordIterator->GoalPosition, Color);
		xr_sprintf(Label, "#%u %s | %.1f m/s", RecordIterator->OwnerIdentifier, RecordIterator->State.c_str(), RecordIterator->Speed);
		HUD().world_prims.append_text3d(RecordIterator->Position, Label, Color);
	}
}
