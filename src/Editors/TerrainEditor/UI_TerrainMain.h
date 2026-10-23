#pragma once

class CTerrainMain : public TUI
{
	typedef TUI inherited;

	virtual void RealUpdateScene();
	virtual void RealQuit();

public:
	CTerrainMain();
	virtual ~CTerrainMain();

	virtual char* GetCaption();

	virtual void ResetStatus() {}
	virtual void SetStatus(const char* s, bool bOutLog);
	virtual void ProgressDraw();

	virtual const char* EditorName() { return "terrain"; }
	virtual const char* EditorDesc() { return "Terrain Editor"; }

	virtual bool ApplyShortCut(DWORD Key, TShiftState Shift);
	virtual bool ApplyGlobalShortCut(DWORD Key, TShiftState Shift);

	virtual void RegisterCommands();
};
