#pragma once

class UILeftBarForm : public IEditorWnd
{
public:
	UILeftBarForm() = default;
	virtual ~UILeftBarForm() = default;
	virtual void Draw();

private:
	void DrawProject();
	void DrawBrush();
	void DrawLayers();
	void DrawExport();

	char m_NameBuffer[64] = {};
	xr_vector<xr_string> m_Projects;
	bool m_ProjectsListed = false;
};
