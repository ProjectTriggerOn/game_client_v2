#include <sstream>
#include "debug_ostream.h"
#include "game_window.h"


#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "direct3d.h"
#include "sprite.h"
#include "shader.h"
#include "shader_3d.h"
#include "grid.h"
#include "sprite_anime.h"
#include "debug_text.h"
#include "texture.h"
#include <thread>
#include "system_timer.h"
#include <sstream>
#include "polygon.h"
#include "DirectXMath.h"
#include "key_logger.h"
#include "mouse.h"
#include "collision.h"
#include <Xinput.h>
#pragma comment(lib, "xinput.lib")
#include "game.h"

#include "audio.h"
#include "cube.h"
#include "fade.h"
#include "infinite_grid.h"
#include "light.h"
#include "mesh_field.h"
#include "ms_logger.h"
#include "sampler.h"
#include "scene.h"
#include "shader_3d_ani.h"
#include "shader_3d_unlit.h"
#include "shader_field.h"
#include "shader_infinite.h"

#include "mock_server.h"
#include "mock_network.h"
#include "enet_client_network.h"
#include "input_producer.h"
#include "remote_player.h"
#include "i_network.h"
#include "config.h"
#include "exe_path.h"
#include "debug_log.h"
#include "game.h"
#include "map.h"
#include "mouse_policy.h"
#include "ui_policy.h"
#include "ui_manager.h"
#include "display_manager.h"

#include <timeapi.h>
#pragma comment(lib, "winmm.lib")   // timeBeginPeriod for the frame limiter


//Window procedure prototype claim
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Frame-cap target time (seconds/frame); 0 = uncapped. Driven by display.max_fps.
static double g_TargetFrameTime = 0.0;

// Global accessor for MockServer (for debug visualization in mock mode)
MockServer* g_pMockServer = nullptr;

// Global network interface pointer (used by game.cpp etc.)
INetwork* g_pNetwork = nullptr;

// Network mode: "mock", "local", or "remote" (read from config.toml)
static std::string g_NetworkMode;

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE,[[maybe_unused]] _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	// MUST be first: the shipped build keeps every DLL in bin\ beside the exe,
	// but the loader only searches the exe's OWN directory for imports. Every
	// DLL we import is /DELAYLOAD-ed (see TriggerOn.vcxproj), so none of them
	// has resolved yet — adding bin\ to the process search path here is in time
	// for the first one, and their transitive deps (UltralightCore/WebCore, the
	// VC++ CRT) resolve through the same path.
	//
	// NOTE: LOAD_LIBRARY_SEARCH_DEFAULT_DIRS also drops the CWD and PATH from the
	// search order process-wide, which is why the post-build stages EVERY DLL
	// (Ultralight's and assimp's) into $(TargetDir): assimp used to be found only
	// via the CWD in Debug, and would otherwise fail to delay-load there.
	SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	AddDllDirectory((ExeDirW() + L"bin").c_str());

#if !defined(_DEBUG) && !defined(DEBUG)
	// Release: every asset loader uses a CWD-relative literal
	// ("resource/shader/*.cso", "resource/texture/*.png", "resource/model/*.fbx",
	// "resource/maps/default.map"), so the game silently only worked when the CWD
	// happened to be the exe dir — i.e. an Explorer double-click. A shortcut with
	// a blank "Start in", or any script launch, made every shader load fail.
	// The shipped layout puts all of it beside the exe, so pin the CWD there and
	// the whole class is fixed at once. Debug is excluded — it deliberately runs
	// on VS's CWD, the project dir, where its resource/ and config/ live.
	//
	// A locally-built x64\Release exe works in place too: the post-build now mirrors
	// resource\ into $(TargetDir) as well, so exeDir and the CWD are the same complete
	// layout the ZIP ships. Left unconditional on purpose — probing for resource\ and
	// skipping the pin when it is absent would turn a loud failure into a quiet run on
	// whatever assets some inherited CWD happened to hold.
	SetCurrentDirectoryW(ExeDirW().c_str());
#endif

	(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Load global config early so window size settings are available.
	// Resolve exe-relative first so the shipped build finds config\config.toml
	// no matter the CWD (a shortcut with a blank "Start in", or a script
	// launch); fall back to the CWD-relative path so the Debug/dev workflow —
	// run from game_client/, where config/ lives — still finds it. A truly
	// missing file no longer crashes: Config::Load falls back to built-in
	// defaults. user_settings.toml is written next to the config it overrides.
	{
		const std::string exeDir = ExeDirA();
		std::string cfgPath  = exeDir + "config\\config.toml";
		std::string userPath = exeDir + "config\\user_settings.toml";
		if (!std::filesystem::exists(cfgPath)) {
			cfgPath  = "config\\config.toml";          // CWD (Debug: project dir)
			userPath = "config\\user_settings.toml";
		}
		Config::GetInstance().Load(cfgPath, userPath);
	}

	// Live display keys (Subscribe fires immediately with the persisted value).
	// VSync only sets a static, so it's safe before Direct3D_Initialize; the
	// frame cap only bites when VSync is off (see the limiter at end-of-frame).
	Config::GetInstance().Subscribe("display.vsync", [](const ConfigValue& v) {
		Direct3D_SetVSyncInterval(v.AsBool() ? 1u : 0u);
	});
	Config::GetInstance().Subscribe("display.max_fps", [](const ConfigValue& v) {
		const int cap = (int)v.AsInt();
		g_TargetFrameTime = cap > 0 ? 1.0 / (double)cap : 0.0;
	});

	// Initialize file-only diagnostic logger (no-op if [debug].enable_log = false)
	DebugLog_Initialize();

	HWND hWnd = GameWindow_Generate(hInstance);

	SystemTimer_Initialize();

	Direct3D_Initialize(hWnd);

	KeyLogger_Initialize();

	MSLogger_Initialize(hWnd);

	InitAudio();

	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Shader_3D_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Shader_InfiniteGrid_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Shader_3D_Ani_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Shader_3DUnlit_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Light_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Sampler_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Texture_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Polygon_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Sprite_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	SpriteAnime_Initialize();

	Fade_Initialize();

	// UI (Ultralight): per docs §6.3, init after Fade and before Scene.
	// Use GetClientRect (physical pixels) — see docs §6.4 DPI constraints.
	{
		RECT rc{};
		GetClientRect(hWnd, &rc);
		// Pass the monitor DPI zoom so the UI scales up on high-DPI displays
		// (UI::Initialize derives the effective device scale per-resolution, capped
		// so the CSS viewport doesn't crowd the pages). 100% monitors stay at 1.0.
		const UINT dpi = GetDpiForWindow(hWnd);
		float dpiScale = (dpi > 0) ? (float)dpi / 96.0f : 1.0f;
		if (dpiScale < 1.0f) dpiScale = 1.0f;
		if (dpiScale > 2.0f) dpiScale = 2.0f;   // cap very-high-DPI (250%+) at 2x
		UI::Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		               rc.right - rc.left, rc.bottom - rc.top, dpiScale);
	}

	// Display manager: enumerate monitors/modes and queue the persisted display
	// mode (applied on the first Update, after ShowWindow, with no revert
	// countdown). Needs Direct3D (DXGI device) and UI (UI::Resize) already up.
	Display::Initialize(hWnd);

	// Boot scene selection (config.toml [debug].start_scene = "game" | "title" | "ui_test")
	{
		const std::string bootScene = Config::GetInstance().GetString("debug", "start_scene", "game");
		if      (bootScene == "ui_test") Scene_SetBootScene(SCENE_UI_TEST);
#ifdef EDITOR_ENABLED
		else if (bootScene == "editor")  Scene_SetBootScene(SCENE_EDITOR);
#endif // EDITOR_ENABLED
		else if (bootScene == "title")   Scene_SetBootScene(SCENE_TITLE);
		else                             Scene_SetBootScene(SCENE_GAME);
	}

	Scene_Initialize();

	// ========================================================================
	// Network Mode Selection (from config.toml)
	//   "mock"   — in-process mock server
	//   "local"  — ENet to local machine
	//   "remote" — ENet to remote server
	// ========================================================================

	g_NetworkMode = Config::GetInstance().GetString("network", "mode", "mock");

	static MockNetwork g_MockNetwork;
	static MockServer g_MockServer;
	static ENetClientNetwork g_ENetNetwork;

	// Load the active map once, before either network path uses its colliders.
	if (!Map_LoadFromFile("resource/maps/default.map"))
		OutputDebugStringA("[MAP] WARNING: failed to load resource/maps/default.map\n");

	if (g_NetworkMode == "local" || g_NetworkMode == "remote")
	{
		// ENet mode: pick host from config based on mode
		std::string serverHost = (g_NetworkMode == "remote")
			? Config::GetInstance().GetString("network", "remote_host", "127.0.0.1")
			: Config::GetInstance().GetString("network", "local_host", "127.0.0.1");
		uint16_t serverPort = static_cast<uint16_t>(Config::GetInstance().ServerPort());

		g_ENetNetwork.SetServerAddress(serverHost.c_str(), serverPort);
		g_ENetNetwork.Initialize();
		g_ENetNetwork.SetExpectedMapChecksum(Map_GetCollisionChecksum());
		g_pNetwork = &g_ENetNetwork;
		g_pMockServer = nullptr;
	}
	else
	{
		// Mock mode: local in-process server (default)
		//
		// Register map colliders BEFORE the mock server starts. Game_Initialize
		// also does this on game-scene entry, but when booting into another scene
		// (e.g. ui_test) that hasn't happened yet — the mock server would simulate
		// the player in an EMPTY collision world (no floor), and by the time the
		// game scene starts the authoritative position is thousands of meters
		// below the map (symptom: only sky + FP model visible, skybox jitters
		// from float precision at huge coordinates).
		// Map_RegisterColliders is pure static data and idempotent (Clear+add).
		Map_RegisterColliders(*Game_GetCollisionWorld());

		g_MockNetwork.Initialize();
		g_MockServer.Initialize(&g_MockNetwork, Game_GetCollisionWorld());
		g_pNetwork = &g_MockNetwork;
		g_pMockServer = &g_MockServer;
	}

	// Initialize Input Producer (Client-side input sampling)
	static InputProducer g_InputProducer;
	g_InputProducer.Initialize(g_pNetwork);
	extern InputProducer* g_pInputProducer;
	g_pInputProducer = &g_InputProducer;

	// Initialize Remote Players (pre-allocate MAX_PLAYERS slots, all inactive)
	extern RemotePlayer g_RemotePlayers[];
	extern bool g_RemotePlayerActive[];
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		g_RemotePlayers[i].Initialize({ 0.0f, 0.0f, 0.0f });
		g_RemotePlayers[i].SetActive(false);
		g_RemotePlayerActive[i] = false;
	}

	Cube_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Shader_Field_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	MeshField_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	InfiniteGrid_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());



	Grid_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Collision_DebugInitialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

#if defined(_DEBUG) || defined(DEBUG)

	hal::DebugText dt(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		L"resource/texture/consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(), Direct3D_GetBackBufferHeight(),
		0.0f, 900.0f,
		0, 0,
		0.0f, 16.0f
	);

#endif // _DEBUG || DEBUG

	ShowWindow(hWnd, nCmdShow);

	UpdateWindow(hWnd);

	Mouse_SetVisible(true);

	// Raise the timer resolution so Sleep(1) in the frame limiter is ~1ms, not
	// the default ~15ms (paired with timeEndPeriod after the loop).
	timeBeginPeriod(1);

	double exec_last_time = SystemTimer_GetTime();
	double fps_last_time = exec_last_time;
	double current_time = 0.0;
	ULONG frame_count = 0;
	double fps = 0.0;


	MSG msg;


	do{
		// W message pump: WM_CHAR encoding is decided by the function that
		// RETRIEVES the message — the A variant converts character messages of a
		// Unicode window to ANSI (non-ASCII becomes '?')
		if (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		else {

			current_time = SystemTimer_GetTime();
			double elapsed_time = current_time - fps_last_time;
			
			if (elapsed_time >= 1.0)
			{
				fps = frame_count / elapsed_time;
				fps_last_time = current_time;
				frame_count = 0;
			}

			elapsed_time = current_time - exec_last_time;
			exec_last_time = current_time;

			KeyLogger_Update();
			MSLogger_Update();

			// UI input: drain UI_InputQueue and dispatch to Ultralight per
			// InteractiveLevel. Runs every frame in all scenes.
			// (F1/F2 page-preview shortcuts live in SCENE_UI_TEST's UITest_Update)
			UI::ProcessInput();

			Scene_Update(elapsed_time);

			// Derive cursor (mouse_policy) and UI input level + page (ui_policy)
			// from (scene, GameState). Both run after Scene_Update so this frame's
			// state flips are reflected; both are the SOLE owners of their domains.
			MousePolicy_Apply();
			UIPolicy_Apply();

			// Input sampling and mock-server simulation only run during gameplay.
			// In other scenes (ui_test): typing WASD into a UI text field must not
			// drive the server-side player, and the mock world stays frozen.
			// InputProducer itself zeroes movement/buttons when !Game_IsGameplayActive
			// (e.g. paused); the scene gate here also freezes the mock simulation.
			// ENet PollEvents always runs to keep the connection alive.
			const bool inGameScene = (Scene_GetCurrent() == SCENE_GAME);

			if (inGameScene)
			{
				g_InputProducer.Update(elapsed_time);
			}

			if (g_NetworkMode == "local" || g_NetworkMode == "remote")
			{
				g_ENetNetwork.PollEvents();
			}
			else if (inGameScene)
			{
				g_MockServer.Update(elapsed_time);
			}

			Fade_Update(elapsed_time);

			// Advance the masked scene-transition coordinator (curtain fade +
			// scene swap). Runs before Scene_Refresh so a swap it requests this
			// frame is applied at end-of-frame, behind the opaque curtain.
			SceneTransition_Update(elapsed_time);

			SpriteAnime_Update(elapsed_time);

			// Apply any staged display change + drive the revert countdown. Runs
			// before Direct3D_Clear so a swap-chain resize precedes this frame's
			// draw (SetWindowPos here fires WM_SIZE → Display::OnWindowResize).
			Display::Update(elapsed_time);

			Direct3D_Clear();

			Sprite_Begin();

			Scene_Draw();

			UI::Render();   // above 3D/2D

			// Native fade overlay (in-game death/respawn tint), drawn above the UI
			// for a full-screen effect. Suppressed during a masked scene transition
			// so it can't paint over the loading curtain — the curtain owns the
			// transition visual (and SceneTransition_To clears any lingering fade).
			if (!SceneTransition_IsActive())
				Fade_Draw();





#if defined(_DEBUG) || defined(DEBUG)


#endif // _DEBUG || DEBUG

			Direct3D_Present();

			Scene_Refresh();

			frame_count++;

			// Frame cap (display.max_fps): only meaningful with VSync off. Coarse
			// Sleep down to the last ~2ms, then spin. exec_last_time is this
			// frame's start timestamp (set above).
			if (g_TargetFrameTime > 0.0) {
				const double frameEnd = exec_last_time + g_TargetFrameTime;
				for (;;) {
					const double now = SystemTimer_GetTime();
					const double remain = frameEnd - now;
					if (remain <= 0.0) break;
					if (remain > 0.002) Sleep(1);
				}
			}
		}
	} while (msg.message != WM_QUIT);

	timeEndPeriod(1);

	// Network cleanup
	if (g_NetworkMode == "local" || g_NetworkMode == "remote")
	{
		g_ENetNetwork.Finalize();
	}
	else
	{
		g_MockServer.Finalize();
		g_MockNetwork.Finalize();
	}
	g_pNetwork = nullptr;

#if defined(_DEBUG) || defined(DEBUG)

	Collision_DebugFinalize();

#endif

	Cube_Finalize();

	Scene_Finalize();

	// Leave exclusive fullscreen + release DXGI enum before Direct3D_Finalize.
	Display::Finalize();

	UI::Finalize();

	Fade_Finalize();

	SpriteAnime_Finalize();

	Texture_Finalize();

	Sprite_Finalize();

	Texture_AllRelease();

	Shader_Finalize();

	Shader_3D_Finalize();

	Shader_InfiniteGrid_Finalize();

	Shader_3DUnlit_Finalize();

	Light_Finalize();

	Sampler_Finalize();

	InfiniteGrid_Finalize();

	Grid_Finalize();

	Shader_3D_Ani_Finalize();

	Polygon_Finalize();

	Direct3D_Finalize();

	Mouse_Finalize();

	DebugLog_Finalize();

	return static_cast<int>(msg.wParam);

}


