#include "stdafx.h"
#include "ai/ExtendedEntityesLogic/Debug/DebugCommands.h"
#include "ai/ExtendedEntityesLogic/Debug/Debug.h"
#include "ai/ExtendedEntityesLogic/CELSharedSettings.h"

// Применяет копии консольных режимов и однократный запрос сброса статистики.
void CELSharedDebugCommands::Synchronize()
{
	static u32 LogicRevision = 0;
	static u32 StatsResetRevision = 0;
	SControl LogicControl;
	if (Read("monster_logic", LogicControl) && (LogicControl.Mode != CELSharedSettings::GetOverride() || LogicControl.Revision != LogicRevision))
	{
		CELSharedSettings::SetEnabled(LogicControl.Mode);
		LogicRevision = LogicControl.Revision;
	}
	SControl DrawControl;
	if (Read("monster_logic_draw", DrawControl))
	{
		CELSharedDebug::Instance().SetDrawEnabled(DrawControl.Mode != 0);
	}
	SControl StatsControl;
	if (Read("monster_logic_stats", StatsControl))
	{
		if (StatsControl.ResetRevision != StatsResetRevision)
		{
			CELSharedDebug::Instance().Clear();
			StatsResetRevision = StatsControl.ResetRevision;
		}
		CELSharedDebug::Instance().SetStatsEnabled(StatsControl.Mode != 0);
	}
}
