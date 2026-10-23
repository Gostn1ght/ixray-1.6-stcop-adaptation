#include "stdafx.h"

namespace
{
	constexpr int Resolutions[] = { 129, 257, 513, 1025, 2049 };
	constexpr const char* ResolutionNames[] = { "129", "257", "513", "1025", "2049" };
	constexpr int ExportSteps[] = { 1, 2, 4 };
	constexpr const char* ExportStepNames[] = { "1 (full)", "2", "4" };
	constexpr int TexSizes[] = { 1024, 2048, 4096 };
	constexpr const char* TexSizeNames[] = { "1024", "2048", "4096" };

	template<size_t N>
	int IndexOf(const int (&values)[N], int value)
	{
		for (size_t i = 0; i < N; ++i)
			if (values[i] == value)
				return int(i);
		return 0;
	}

	void InputString(const char* label, xr_string& value)
	{
		char buffer[256];
		xr_strcpy(buffer, value.c_str());
		if (ImGui::InputText(label, buffer, sizeof(buffer)))
			value = buffer;
	}
}

void UILeftBarForm::Draw()
{
	if (ImGui::Begin("Terrain", nullptr))
	{
		DrawProject();
		DrawBrush();
		DrawLayers();
		DrawExport();

		ImGui::Separator();
		if (TTools->CursorValid())
		{
			const Fvector& c = TTools->Cursor();
			ImGui::Text("Cursor: %.1f, %.1f  height %.2f m", c.x, c.z, c.y);
		}
		ImGui::TextDisabled("LMB - brush, Shift+mouse - camera");
	}
	ImGui::End();
}

void UILeftBarForm::DrawProject()
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (!ImGui::CollapsingHeader("Project"))
		return;

	CTerrainData& data = TTools->Data;
	xr_strcpy(m_NameBuffer, data.Name.c_str());
	if (ImGui::InputText("Name", m_NameBuffer, sizeof(m_NameBuffer), ImGuiInputTextFlags_CharsNoBlank))
	{
		data.Name = m_NameBuffer;
		TTools->Modified();
	}

	ImGui::Text("Grid %u x %u, %.0f m, cell %.2f m", data.Resolution, data.Resolution, data.Size, data.IsValid() ? data.CellSize() : 0.f);

	if (ImGui::TreeNode("New terrain"))
	{
		int res = IndexOf(Resolutions, TTools->NewResolution);
		if (ImGui::Combo("Resolution", &res, ResolutionNames, IM_ARRAYSIZE(ResolutionNames)))
			TTools->NewResolution = Resolutions[res];
		ImGui::InputFloat("Size, m", &TTools->NewSize, 16.f, 128.f, "%.0f");
		ImGui::InputFloat("Base height, m", &TTools->NewHeight, 1.f, 10.f, "%.1f");
		TTools->NewSize = std::clamp(TTools->NewSize, 16.f, 8192.f);

		if (ImGui::Button("Create"))
			ExecCommand(COMMAND_CLEAR);
		ImGui::TreePop();
	}

	if (ImGui::Button("Save"))
	{
		ExecCommand(COMMAND_SAVE);
		m_ProjectsListed = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Undo"))
		TTools->Undo();
	ImGui::SameLine();
	if (ImGui::Button("Redo"))
		TTools->Redo();

	if (ImGui::TreeNode("Open"))
	{
		if (!m_ProjectsListed || ImGui::Button("Refresh"))
		{
			TTools->ListProjects(m_Projects);
			m_ProjectsListed = true;
		}

		for (const xr_string& name : m_Projects)
		{
			if (ImGui::Selectable(name.c_str()))
				TTools->LoadProject(name.c_str());
		}
		ImGui::TreePop();
	}
}

void UILeftBarForm::DrawBrush()
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (!ImGui::CollapsingHeader("Brush"))
		return;

	int brush = int(TTools->Brush);
	ImGui::RadioButton("Raise", &brush, int(ETerrainBrush::Raise)); ImGui::SameLine();
	ImGui::RadioButton("Lower", &brush, int(ETerrainBrush::Lower)); ImGui::SameLine();
	ImGui::RadioButton("Smooth", &brush, int(ETerrainBrush::Smooth));
	ImGui::RadioButton("Flatten", &brush, int(ETerrainBrush::Flatten)); ImGui::SameLine();
	ImGui::RadioButton("Noise", &brush, int(ETerrainBrush::Noise)); ImGui::SameLine();
	ImGui::RadioButton("Paint", &brush, int(ETerrainBrush::Paint));
	TTools->Brush = ETerrainBrush(brush);

	ImGui::SliderFloat("Radius, m", &TTools->BrushRadius, 0.5f, 200.f, "%.1f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Strength", &TTools->BrushStrength, 0.05f, 50.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Falloff", &TTools->BrushFalloff, 0.f, 1.f, "%.2f");

	if (TTools->Brush == ETerrainBrush::Paint)
	{
		CTerrainData& data = TTools->Data;
		for (int l = 0; l < int(CTerrainData::LayerCount); ++l)
		{
			if (l)
				ImGui::SameLine();
			ImGui::RadioButton(data.Layers[l].Name.c_str(), &TTools->PaintLayer, l);
		}
	}
}

void UILeftBarForm::DrawLayers()
{
	if (!ImGui::CollapsingHeader("Layers"))
		return;

	CTerrainData& data = TTools->Data;
	for (u32 l = 0; l < CTerrainData::LayerCount; ++l)
	{
		ImGui::PushID(int(l));
		STerrainLayer& layer = data.Layers[l];

		float color[3] = { color_get_R(layer.PreviewColor) / 255.f, color_get_G(layer.PreviewColor) / 255.f, color_get_B(layer.PreviewColor) / 255.f };
		if (ImGui::ColorEdit3("##color", color, ImGuiColorEditFlags_NoInputs))
		{
			layer.PreviewColor = color_rgba(u32(color[0] * 255.f), u32(color[1] * 255.f), u32(color[2] * 255.f), 255);
			TTools->Modified();
			TTools->InvalidateRender();
		}
		ImGui::SameLine();
		InputString("##name", layer.Name);
		ImGui::PopID();
	}
	ImGui::TextDisabled("Mask channels: R, G, B, A = layers 1-4");
}

void UILeftBarForm::DrawExport()
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (!ImGui::CollapsingHeader("Export"))
		return;

	CTerrainData& data = TTools->Data;
	InputString("Shader", data.Shader);
	InputString("Game material", data.GameMtl);
	InputString("Thm template", data.ThmTemplate);

	int step = IndexOf(ExportSteps, TTools->ExportStep);
	if (ImGui::Combo("Mesh step", &step, ExportStepNames, IM_ARRAYSIZE(ExportStepNames)))
		TTools->ExportStep = ExportSteps[step];

	int tex = IndexOf(TexSizes, TTools->ExportTexSize);
	if (ImGui::Combo("Texture size", &tex, TexSizeNames, IM_ARRAYSIZE(TexSizeNames)))
		TTools->ExportTexSize = TexSizes[tex];

	ImGui::TextDisabled("Object: %s", TTools->ObjectRef().c_str());

	if (ImGui::Button("Export"))
		TTools->Export();
	ImGui::SameLine();
	if (ImGui::Button("Open in Level Editor"))
		TTools->OpenInLevelEditor();
}
