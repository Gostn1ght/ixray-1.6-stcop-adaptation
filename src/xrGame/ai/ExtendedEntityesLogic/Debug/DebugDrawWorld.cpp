#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/Debug/DebugDrawWorld.h"
#include "HUDManager.h"

// Добавляет маршруты, цели и индикаторы атаки в очередь HUD().world_prims.
void CELSharedDebugDrawWorld::Draw(const xr_vector<SELDebugRecord>& Records, const xr_vector<SELDebugEvent>& Events)
{
	xr_vector<SELDebugRecord>::const_iterator RecordIterator;
	u32 Color;
	string256 Label;
	for (RecordIterator = Records.begin(); RecordIterator != Records.end(); ++RecordIterator)
	{
		Color = RecordIterator->IsAttacking ? color_rgba(255, 30, 30, 180) : color_rgba(255, 220, 40, 160);
		HUD().world_prims.append_sphere(RecordIterator->Position, RecordIterator->Radius, Color);
		HUD().world_prims.append_line(RecordIterator->Position, RecordIterator->GoalPosition, Color);
		xr_sprintf(Label, "#%u %s | %.1f м/с", RecordIterator->OwnerIdentifier, RecordIterator->State.c_str(), RecordIterator->Speed);
		HUD().world_prims.append_text3d(RecordIterator->Position, Label, Color);
	}
	xr_vector<SELDebugEvent>::const_iterator Event;
	u32 PointIndex;
	for (Event = Events.begin(); Event != Events.end(); ++Event)
	{
		switch (Event->Kind)
		{
			case EELDebugEvent::Sound: Color = color_rgba(50, 160, 255, 160); break;
			case EELDebugEvent::Interest: Color = color_rgba(80, 220, 255, 180); break;
			case EELDebugEvent::Fear: Color = color_rgba(255, 80, 60, 180); break;
			case EELDebugEvent::Sharing: Color = color_rgba(190, 80, 255, 180); break;
			case EELDebugEvent::Call: Color = color_rgba(70, 255, 180, 160); break;
			case EELDebugEvent::Trail: Color = color_rgba(255, 150, 30, 180); break;
			case EELDebugEvent::Search: Color = color_rgba(255, 220, 40, 160); break;
			case EELDebugEvent::Hazard: Color = color_rgba(255, 60, 80, 160); break;
			default: Color = color_rgba(220, 100, 255, 160); break;
		}
		HUD().world_prims.append_line(Event->Start, Event->Finish, Color);
		if (Event->Radius > 0.f)
		{
			HUD().world_prims.append_sphere(Event->Finish, Event->Radius, Color);
		}
		for (PointIndex = 1; PointIndex < Event->Points.size(); ++PointIndex)
		{
			HUD().world_prims.append_line(Event->Points[PointIndex - 1], Event->Points[PointIndex], Color);
		}
		if (Event->Label.size())
		{
			if (Event->TargetIdentifier == u16(-1))
			{
				xr_sprintf(Label, "#%u | %s", Event->OwnerIdentifier, Event->Label.c_str());
			}
			else
			{
				xr_sprintf(Label, "#%u -> #%u | %s", Event->OwnerIdentifier, Event->TargetIdentifier, Event->Label.c_str());
			}
			HUD().world_prims.append_text3d(Event->Finish, Label, Color);
		}
	}
}
