#include "stdafx.h"
#include "../xrECore/Editor/imgui_EditorEx.h"

#include "IconsFontAwesome6.h"

void UIMainMenuForm::Draw()
{
	ImGui::PushStyleColor(ImGuiCol_Button, XRay::ImGui::GetEditorColor(XRay::ImGui::EEditorColors::BackgroundTint).Value);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	if (IXBeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItemI("New", ICON_FA_FILE, ""))
				ExecCommand(COMMAND_CLEAR);
			if (ImGui::MenuItemI("Save", ICON_FA_FLOPPY_DISK, ""))
				ExecCommand(COMMAND_SAVE);
			ImGui::Separator();
			if (ImGui::MenuItemI("Export", ICON_FA_FILE_EXPORT, ""))
				TTools->Export();
			if (ImGui::MenuItemI("Open in Level Editor", ICON_FA_MOUNTAIN, ""))
				TTools->OpenInLevelEditor();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "", false, TTools->CanUndo()))
				ExecCommand(COMMAND_UNDO);
			if (ImGui::MenuItem("Redo", "", false, TTools->CanRedo()))
				ExecCommand(COMMAND_REDO);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			bool wire = EDevice->dwFillMode == D3DFILL_WIREFRAME;
			if (ImGui::MenuItem("Wireframe", "", &wire))
			{
				EDevice->dwFillMode = wire ? D3DFILL_WIREFRAME : D3DFILL_SOLID;
				UI->RedrawScene();
			}

			bool grid = psDeviceFlags.test(rsDrawGrid);
			if (ImGui::MenuItemI("Draw Grid", ICON_FA_TABLE_CELLS, "", &grid))
			{
				psDeviceFlags.set(rsDrawGrid, grid);
				UI->RedrawScene();
			}

			if (ImGui::MenuItem("Zoom to Terrain"))
				TTools->ZoomObject(false);

			bool log = AllowLogCommands();
			if (ImGui::MenuItem("Log", "", &log))
				ExecCommand(COMMAND_LOG_COMMANDS);
			ImGui::EndMenu();
		}

		IXEndMainMenuBar();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}
