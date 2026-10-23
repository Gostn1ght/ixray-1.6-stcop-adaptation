// TerrainEditor.cpp : terrain sculpting and painting tool of the NetAnomaly SDK
//
#include "stdafx.h"
#include "../../xrEngine/xr_input.h"

#include "../xrPlay/Splash.h"

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* pCmdLine, int nCmdShow)
{
	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS))
	{
		Msg("! SDL_Init Error: %s", SDL_GetError());
		return 0;
	}

	splash::SetBackground(IDB_LE);
	std::jthread s(splash::Show);

	splash::SetProgressStatus(5, "Initializing Debugger");

	Debug._initialize(false);

	const char* FSName = "fs.ltx";
	const char* fsgame_ltx_name = "-fsltx ";
	string_path fsgame = "";

	if (strstr(pCmdLine, fsgame_ltx_name))
	{
		int sz = xr_strlen(fsgame_ltx_name);
		sscanf(strstr(pCmdLine, fsgame_ltx_name) + sz, "%[^ ] ", fsgame);
	}

	splash::SetProgressStatus(10, "Initializing COM Library");

	CoInitialize(nullptr);

	splash::SetProgressStatus(20, "Core Initialization");

	Core._initialize("Terrain", ELogCallback, 1, fsgame[0] ? fsgame : FSName);

	psDeviceFlags.set(rsFullscreen, false);

	splash::SetProgressStatus(35, "Initializing Terrain Tools");
	Tools = new CTerrainTool();

	splash::SetProgressStatus(55, "Registering UI Commands");

	UI = new CTerrainMain();
	UI->RegisterCommands();

	splash::SetProgressStatus(75, "Creating Main UI Form");

	UIMainForm* MainForm = new UIMainForm();
	::MainForm = MainForm;
	UI->Push(MainForm, false);

	splash::SetProgressStatus(100, "Finalizing");
	splash::Close();

	bool NeedExit = false;
	while (!NeedExit)
	{
		SDL_Event Event;
		while (SDL_PollEvent(&Event))
		{
			switch (Event.type)
			{
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			{
				SDL_WindowID MainWndID = SDL_GetWindowID(g_AppInfo.Window);
				if (Event.window.windowID == MainWndID && Tools->IfModified())
				{
					EPrefs->SaveConfig();
					NeedExit = true;
				}
				break;
			}
			case SDL_EVENT_WINDOW_RESIZED:
			{
				SDL_WindowID MainWndID = SDL_GetWindowID(g_AppInfo.Window);
				if (UI && REDevice && Event.window.windowID == MainWndID)
				{
					UI->Resize(Event.window.data1, Event.window.data2, true);
					EPrefs->SaveConfig();
				}
				break;
			}
			case SDL_EVENT_WINDOW_SHOWN:
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
				Device.b_is_Active = true;
				break;
			case SDL_EVENT_WINDOW_HIDDEN:
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				Device.b_is_Active = false;
				break;

			case SDL_EVENT_KEY_DOWN:
				if (UI)
				{
					UI->KeyDown(Event.key.scancode, UI->GetShiftState());
					UI->ApplyShortCutInput(Event.key.scancode);
				}
				break;
			case SDL_EVENT_KEY_UP:
				if (UI)
					UI->KeyUp(Event.key.scancode, UI->GetShiftState());
				break;

			case SDL_EVENT_MOUSE_MOTION:
				pInput->MouseMotion(Event.motion.xrel, Event.motion.yrel);
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				pInput->MouseScroll(Event.wheel.y);
				break;

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				int mouse_button = 0;
				if (Event.button.button == SDL_BUTTON_LEFT) { mouse_button = 0; }
				if (Event.button.button == SDL_BUTTON_RIGHT) { mouse_button = 1; }
				if (Event.button.button == SDL_BUTTON_MIDDLE) { mouse_button = 2; }
				if (Event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
					pInput->MousePressed(mouse_button);
				else
					pInput->MouseReleased(mouse_button);
			}
			break;
			}

			if (!UI->ProcessEvent(&Event))
				break;
		}
		MainForm->Frame();
	}
	s.join();
	xr_delete(MainForm);
	Core._destroy();
	return 0;
}
