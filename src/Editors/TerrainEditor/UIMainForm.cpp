#include "stdafx.h"
#include "imgui_internal.h"

UIMainForm* MainForm = nullptr;

namespace
{
	// First run without a saved layout: terrain panel on the left, viewport in the middle, log below
	void SetupDefaultLayout()
	{
		if (ImGui::FindWindowSettingsByID(ImHashStr("Terrain")))
			return;

		const ImGuiID dockspace = ImHashStr("MyDockspace", 0, ImHashStr("MyDockspace"));
		ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspace);
		if (!root || root->IsSplitNode())
			return;

		ImGuiID left = 0, rest = 0, bottom = 0, center = 0;
		ImGui::DockBuilderSplitNode(dockspace, ImGuiDir_Left, 0.24f, &left, &rest);
		ImGui::DockBuilderSplitNode(rest, ImGuiDir_Down, 0.2f, &bottom, &center);
		ImGui::DockBuilderDockWindow("Terrain", left);
		ImGui::DockBuilderDockWindow("Render##0", center);
		ImGui::DockBuilderDockWindow("Log", bottom);
		ImGui::DockBuilderFinish(dockspace);
	}
}

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
	static bool layout_checked = false;
	if (!layout_checked)
	{
		layout_checked = true;
		SetupDefaultLayout();
	}

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
