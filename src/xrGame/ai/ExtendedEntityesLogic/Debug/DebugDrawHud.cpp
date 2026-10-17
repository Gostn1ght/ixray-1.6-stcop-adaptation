#include "stdafx.h"
#include "DebugDrawHud.h"
#include "HUDManager.h"

// Показывает только цену новой механики: среднее и пик суммы замеров за кадр.
void CELSharedDebugDrawHud::Draw(const xr_vector<SELDebugRecord>& Records, const SELDebugFrame& Frame, const SELDebugPerformance& Performance)
{
	string256 Label;
	const u32 Color = color_rgba(80, 255, 80, 255);
	xr_sprintf(Label, "Логика: %s | ГГ: %s | мс/кадр: среднее / пик", Frame.IsLogicEnabled ? "ВКЛ" : "ШТАТНАЯ", Frame.IgnoresActor ? "игнорируется" : "учитывается");
	HUD().world_prims.append_text2d(20.f, 100.f, Label, Color, CGameFont::alLeft);
	const char* Names[] = {"Наземные", "Летуны", "Общие службы"};
	u32 GroupIndex;
	for (GroupIndex = 0; GroupIndex < u32(EELDebugGroup::Count); ++GroupIndex)
	{
		xr_sprintf(Label, "%s: %.3f / %.3f", Names[GroupIndex], Performance.Groups[GroupIndex].AverageMilliseconds, Performance.Groups[GroupIndex].PeakMilliseconds);
		HUD().world_prims.append_text2d(20.f, 118.f + float(GroupIndex) * 18.f, Label, Color, CGameFont::alLeft);
	}
	xr_sprintf(Label, "Обновление существ и служб: %.3f / %.3f", Performance.Main.AverageMilliseconds, Performance.Main.PeakMilliseconds);
	HUD().world_prims.append_text2d(20.f, 172.f, Label, Color, CGameFont::alLeft);
	xr_sprintf(Label, "Рабочие задания: %.3f / %.3f", Performance.Worker.AverageMilliseconds, Performance.Worker.PeakMilliseconds);
	HUD().world_prims.append_text2d(20.f, 190.f, Label, Color, CGameFont::alLeft);
	xr_sprintf(Label, "ВСЕГО (обновление + задания): %.3f / %.3f", Performance.Total.AverageMilliseconds, Performance.Total.PeakMilliseconds);
	HUD().world_prims.append_text2d(20.f, 208.f, Label, Color, CGameFont::alLeft);
}
