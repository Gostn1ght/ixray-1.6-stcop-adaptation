#pragma once
class UILeftBarForm;
class UIRenderForm;
class UIMainMenuForm;

class UIMainForm : public IEditorWnd
{
public:
	UIMainForm();
	virtual ~UIMainForm();
	virtual void Draw();
	bool Frame();

private:
	UIRenderForm* m_Render;
	UIMainMenuForm* m_MainMenu;
	UILeftBarForm* m_LeftBar;
};
extern UIMainForm* MainForm;
