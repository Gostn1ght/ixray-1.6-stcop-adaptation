#include "stdafx.h"
#include "UI_TerrainMain.h"

CTerrainMain::CTerrainMain()
{
	EPrefs = new CCustomPreferences();
}

CTerrainMain::~CTerrainMain()
{
	xr_delete(EPrefs);
}

void CTerrainMain::RegisterCommands()
{
	inherited::RegisterCommands();

	REGISTER_CMD_CE(COMMAND_SAVE, "File\\Save", TTools, CTerrainTool::CommandSave, true);
	REGISTER_CMD_CE(COMMAND_CLEAR, "File\\New", TTools, CTerrainTool::CommandClear, true);
	REGISTER_CMD_CE(COMMAND_UNDO, "Edit\\Undo", TTools, CTerrainTool::CommandUndo, true);
	REGISTER_CMD_CE(COMMAND_REDO, "Edit\\Redo", TTools, CTerrainTool::CommandRedo, true);
}

char* CTerrainMain::GetCaption()
{
	return (char*)"terrain";
}

void CTerrainMain::SetStatus(const char* s, bool bOutLog)
{
	if (bOutLog && s && s[0])
		ELog.Msg(mtInformation, s);
}

void CTerrainMain::ProgressDraw()
{
	inherited::ProgressDraw();
}

bool CTerrainMain::ApplyShortCut(DWORD Key, TShiftState Shift)
{
	return inherited::ApplyShortCut(Key, Shift);
}

bool CTerrainMain::ApplyGlobalShortCut(DWORD Key, TShiftState Shift)
{
	return inherited::ApplyGlobalShortCut(Key, Shift);
}

void CTerrainMain::RealUpdateScene()
{
	inherited::RealUpdateScene();
}

void CTerrainMain::RealQuit()
{
	inherited::Quit();
}
