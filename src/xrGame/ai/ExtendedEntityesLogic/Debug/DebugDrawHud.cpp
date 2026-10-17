#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/Debug/DebugDrawHud.h"
#include "HUDManager.h"

// Рисует общий снимок статистики и сведения о выбранной сущности на HUD.
void CELSharedDebugDrawHud::Draw(const xr_vector<SELDebugRecord>& Records, const SELDebugFrame& Frame)
{
	string256 Label;
	xr_sprintf(Label, "Extended entityes: %s | owners %u | records %u", Frame.IsLogicEnabled ? "ON" : "NATIVE", Frame.OwnerCount, u32(Records.size()));
	HUD().world_prims.append_text2d(20.f, 100.f, Label, color_rgba(80, 255, 80, 255), CGameFont::alLeft);
	xr_sprintf(Label, "Requests: worker %u | graph %u | geometry %u | generation %u | last worker %.3f ms", Frame.WorkerRequests, Frame.GraphCaptures, Frame.GeometryChecks, Frame.WorldGeneration, float(Frame.LastWorkerMicroseconds) * .001f);
	HUD().world_prims.append_text2d(20.f, 118.f, Label, color_rgba(80, 255, 80, 255), CGameFont::alLeft);
	const SELDebugRecord* Focus = nullptr;
	float BestAlignment = .98f;
	float Alignment;
	Fvector Direction;
	xr_vector<SELDebugRecord>::const_iterator RecordIterator;
	for (RecordIterator = Records.begin(); RecordIterator != Records.end(); ++RecordIterator)
	{
		Direction.sub(RecordIterator->Position, Device.vCameraPosition).normalize_safe();
		Alignment = Direction.dotproduct(Device.vCameraDirection);
		if (Alignment > BestAlignment)
		{
			BestAlignment = Alignment;
			Focus = &*RecordIterator;
		}
	}
	if (!Focus)
	{
		return;
	}
	xr_sprintf(Label, "#%u | target %u | memory %u | speed %.2f m/s | state %u", Focus->OwnerIdentifier, Focus->TargetIdentifier, Focus->KnownTargets, Focus->Speed, u32(Focus->Validation));
	HUD().world_prims.append_text2d(20.f, 148.f, Label, color_rgba(255, 220, 40, 255), CGameFont::alLeft);
	xr_sprintf(Label, "%s%s", Focus->State.c_str(), Focus->IsAttacking ? " | HIT WINDOW" : "");
	HUD().world_prims.append_text2d(20.f, 166.f, Label, color_rgba(255, 220, 40, 255), CGameFont::alLeft);
}
