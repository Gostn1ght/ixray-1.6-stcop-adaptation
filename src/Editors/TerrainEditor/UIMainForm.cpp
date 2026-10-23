#include "stdafx.h"

UIMainForm* MainForm = nullptr;

UIMainForm::UIMainForm()
{
	EnableReceiveCommands();
	if (!ExecCommand(COMMAND_INITIALIZE, (u32)0, (u32)0))
	{
		xrLogger::FlushLog();
		exit(-1);
	}
	ExecCommand(COMMAND_UPDATE_GRID);
	ExecCommand(COMMAND_RENDER_FOCUS);

	m_Render = new UIRenderForm();
	m_MainMenu = new UIMainMenuForm();
	m_LeftBar = new UILeftBarForm();
}

UIMainForm::~UIMainForm()
{
	xr_delete(m_LeftBar);
	xr_delete(m_MainMenu);
	xr_delete(m_Render);
	ExecCommand(COMMAND_DESTROY, (u32)0, (u32)0);
}

void UIMainForm::Draw()
{
	m_MainMenu->Draw();
	m_LeftBar->Draw();
	m_Render->Draw();
}

bool UIMainForm::Frame()
{
	if (UI)
		return UI->Idle();
	return false;
}
