// LevelEditor.cpp : Определяет точку входа для приложения.
//
#include "stdafx.h"

#include "Engine/XrGameManager.h"
#include "Engine/XRayEditor.h"

#include "Editor/Utils/ContentView.h"
#include "Editor/Scene/LEPhysics.h"
#include "UI/UIEditLibrary.h"

#include "../../xrPlay/Splash.h"

#include "../../xrEngine/std_classes.h"
#include "../../xrEngine/IGame_Persistent.h"
#include "../../xrEngine/XR_IOConsole.h"
#include "../../xrEngine/IGame_Level.h"
#include "../../xrEngine/string_table.h"
#include "../../xrEngine/x_ray.h"
#include "../../xrEngine/xr_input.h"
#include "../../xrEngine/FPSCounter.h"

ECORE_API extern bool bIsLevelEditor;
void DragDrop(const xr_string&, int);

// Command line actions of the NetAnomaly SDK: -make_lods, -scene, -add_object
static void RunStartupCommands()
{
	// -make_lods <library folder>: batch "Make LOD (HQ)" for the folder's MU objects
	if (strstr(Core.Params, "-make_lods ")) {
		string_path lod_folder;
		sscanf(strstr(Core.Params, "-make_lods ") + 11, "%[^ ] ", lod_folder);
		UIEditLibrary::MakeLODsInFolder(lod_folder, true);
	}

	// -scene <name.level>: open a scene from $maps$ right after startup
	string_path scene_path = {};
	if (strstr(Core.Params, "-scene ")) {
		string_path scene_name;
		sscanf(strstr(Core.Params, "-scene ") + 7, "%[^ ] ", scene_name);
		FS.update_path(scene_path, _maps_, scene_name);
		if (FS.exist(scene_path))
			ExecCommand(COMMAND_LOAD, xr_string(scene_path));
	}

	// -add_object <library object>: put the object at the origin (once) and save the -scene file,
	// used by the Terrain Editor to hand over an exported terrain
	if (strstr(Core.Params, "-add_object ")) {
		string_path object_ref;
		sscanf(strstr(Core.Params, "-add_object ") + 12, "%[^ ] ", object_ref);
		LUI->LoaderEvent.wait();

		bool present = false;
		for (CCustomObject* O : Scene->ListObj(OBJCLASS_SCENEOBJECT))
		{
			const char* ref = O->RefName();
			if (ref && 0 == xr_strcmp(ref, object_ref))
				present = true;
		}

		if (!present)
		{
			string256 object_name;
			Scene->GenObjectName(OBJCLASS_SCENEOBJECT, object_name, object_ref);
			CSceneObject* obj = new CSceneObject(nullptr, object_name);
			obj->SetReference(object_ref);
			if (obj->Valid())
			{
				obj->SetLoadedState();
				Scene->AppendObject(obj);
				if (scene_path[0])
					ExecCommand(COMMAND_SAVE, xr_string(scene_path), 0);
			}
			else
			{
				ELog.DlgMsg(mtError, "Can't add object '%s' to the scene", object_ref);
				xr_delete(obj);
			}
		}
	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* pCmdLine, int nCmdShow)
{
	bIsLevelEditor = true;

	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		Msg("! SDL_Init Error: %s", SDL_GetError());
		return 0;
	}

	splash::SetBackground(IDB_LE);
	std::jthread s(splash::Show);

	splash::SetProgressStatus(5, "Initializing Debugger");

	Debug._initialize(false);
	
	splash::SetProgressStatus(10, "Initializing Core System");

	const char* FSName = "fs.ltx";
	const char* fsgame_ltx_name = "-fsltx ";
	string_path fsgame = "";

	if (strstr(pCmdLine, fsgame_ltx_name)) {
		int						sz = xr_strlen(fsgame_ltx_name);
		sscanf(strstr(pCmdLine, fsgame_ltx_name) + sz, "%[^ ] ", fsgame);
	}

	Core._initialize("LevelEditor", ELogCallback, 1, fsgame[0] ? fsgame : FSName);

	splash::SetProgressStatus(20, "Initializing Level Tools");

	Tools = new CLevelTool();
	LTools = static_cast<CLevelTool*>(Tools);

	splash::SetProgressStatus(25, "Registering UI Commands");

	UI = new CLevelMain();
	UI->RegisterCommands();
	UI->GeneralTabs.push_back({ "Scene View", nullptr });

	LUI = static_cast<CLevelMain*>(UI);

	splash::SetProgressStatus(30, "Creating Editor Scene");

	Scene = new EScene();
	EditorScene = Scene;

	splash::SetProgressStatus(25, "Initializing Content View");

	GContentView = new CContentView;

	splash::SetProgressStatus(30, "Creating Main UI Form");

	UIMainForm* MainForm = new UIMainForm();

	pApp = new XRayEditor();
	g_XrGameManager = new XrGameManager();
	g_SEFactoryManager = new XrSEFactoryManager();

	splash::SetProgressStatus(40, "Loading Game Materials");

	// Initialize APP
	GameMaterialLibraryEditors->Load();

	splash::SetProgressStatus(55, "Initializing Game Persistent Objects");

	g_pGamePersistent = static_cast<IGame_Persistent*>(g_XrGameManager->Create(CLSID_GAME_PERSISTANT));
	EDevice->seqAppStart.Process<&pureAppStart::OnAppStart>();

	splash::SetProgressStatus(65, "Setting Up Console");

	Console->Execute("default_controls");

	xr_strcpy(Console->ConfigFile, "user_editor.ltx");

	if (strstr(Core.Params, "-ltx ")) {
		string64 c_name;
		sscanf(strstr(Core.Params, "-ltx ") + 5, "%[^ ] ", c_name);
		xr_strcpy(Console->ConfigFile, c_name);
	}

	Console->ExecuteScript(Console->ConfigFile);

	Console->Hide();

	splash::SetProgressStatus(75, "Performing Final UI Setup");

	::MainForm = MainForm;
	UI->Push(MainForm, false);

	pFPSCounter = new XRay::Hardware::FPSCounter();

	bool NeedExit = false;
	splash::SetProgressStatus(85, "Performing Final Checks");
	MainForm->GetRenderForm()->DragFunctor = DragDrop;
	
	splash::SetProgressStatus(90, "Finalizing UI Setup");
	GContentView->Init();
	UI->PushBegin(GContentView);
	splash::SetProgressStatus(100, "Finalizing");
	splash::Close();

	// Startup commands wait until the window has settled: a maximize/resize applied in the
	// middle of a long command resets the device while a frame is still bound
	int StartupFrames = 0;

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
				if (Event.window.windowID == MainWndID)
				{
					GContentView->Destroy();
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
				//if (UI) UI->OnAppActivate();

				break;
			case SDL_EVENT_WINDOW_HIDDEN:
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				Device.b_is_Active = !!psDeviceFlags.test(rsDeviceActive);
				//if (UI)UI->OnAppDeactivate();
				break;

			case SDL_EVENT_KEY_DOWN:
				if (UI)
				{
					UI->KeyDown(Event.key.scancode, UI->GetShiftState());
					UI->ApplyShortCutInput(Event.key.scancode);

					if (UI->IsPlayInEditor())
					{
						if (pInput->IsAcquire)
						{
							pInput->KeyboardButtonUpdate(Event.key.scancode, true);
						}
						else if (Event.key.scancode == SDL_SCANCODE_LALT)
						{
							pInput->acquire();
							UI->IsEnableInput = false;
							ShowCursor(false);
						}
					}
				}break;
			case SDL_EVENT_KEY_UP:
				if (UI) {
					UI->KeyUp(Event.key.scancode, UI->GetShiftState());
					if(UI->IsPlayInEditor() && pInput->IsAcquire) 
					{
						if (pInput->IsAcquire)
						{
							pInput->KeyboardButtonUpdate(Event.key.scancode, false);
						}
					}
				}
				break;
			case SDL_EVENT_MOUSE_MOTION:
			{
				if (UI->IsPlayInEditor() && !pInput->IsAcquire)
					break;

				pInput->MouseMotion(Event.motion.xrel, Event.motion.yrel);
			} break;
			case SDL_EVENT_MOUSE_WHEEL:
			{
				if (UI->IsPlayInEditor() && !pInput->IsAcquire)
					break;

				pInput->MouseScroll(Event.wheel.y);
			}break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				if (UI->IsPlayInEditor() && !pInput->IsAcquire)
					break;

				int mouse_button = 0;
				if (Event.button.button == SDL_BUTTON_LEFT) { mouse_button = 0; }
				if (Event.button.button == SDL_BUTTON_RIGHT) { mouse_button = 1; }
				if (Event.button.button == SDL_BUTTON_MIDDLE) { mouse_button = 2; }
				if (Event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
					pInput->MousePressed(mouse_button);
				}
				else {
					pInput->MouseReleased(mouse_button);
				}
			}
			break;
			}

			if (!UI->ProcessEvent(&Event))
				break;
		}

		MainForm->Frame();

		if (StartupFrames >= 0 && ++StartupFrames == 10)
		{
			StartupFrames = -1;
			RunStartupCommands();
		}

		if (g_pGamePersistent)
			g_pGamePersistent->UpdateParticles();
	}
	s.join();
	xr_delete(g_FontManager);

	g_scene_physics.DestroyAll();
	g_scene_physics.DestroyObjectSpace();

	xr_delete(MainForm);
	//очищение памяти таблицы строк
	CStringTable::Destroy();
	xr_delete(pApp);
	xr_delete(g_XrGameManager);
	xr_delete(g_SEFactoryManager);
	Core._destroy();
	return 0;
}