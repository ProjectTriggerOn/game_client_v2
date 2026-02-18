//#include <Windows.h>
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

#include "Audio.h"
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
#include "game.h"


//Window procedure prototype claim
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Global accessor for MockServer (for debug visualization in mock mode)
MockServer* g_pMockServer = nullptr;

// Global network interface pointer (used by game.cpp etc.)
INetwork* g_pNetwork = nullptr;

// Network mode: "mock", "local", or "remote" (read from config.toml)
static std::string g_NetworkMode;

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE,_In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	HWND hWnd = GameWindow_Generate(hInstance);

	SystemTimer_Initialize();

	Direct3D_Initialize(hWnd);

	KeyLogger_Initialize();

	//Mouse_Initialize(hWnd);

	MSLogger_Initialize(hWnd);

	//Mouse_SetMode(MOUSE_POSITION_MODE_ABSOLUTE);

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

	Scene_Initialize();

	// ========================================================================
	// Network Mode Selection (from config.toml)
	//   "mock"   — in-process mock server
	//   "local"  — ENet to local machine
	//   "remote" — ENet to remote server
	// ========================================================================

	// Load global config
	Config::GetInstance().Load("config.toml");
	g_NetworkMode = Config::GetInstance().GetString("network", "mode", "mock");

	static MockNetwork g_MockNetwork;
	static MockServer g_MockServer;
	static ENetClientNetwork g_ENetNetwork;

	if (g_NetworkMode == "local" || g_NetworkMode == "remote")
	{
		// ENet mode: pick host from config based on mode
		std::string serverHost = (g_NetworkMode == "remote")
			? Config::GetInstance().GetString("network", "remote_host", "127.0.0.1")
			: Config::GetInstance().GetString("network", "local_host", "127.0.0.1");
		uint16_t serverPort = static_cast<uint16_t>(Config::GetInstance().ServerPort());

		g_ENetNetwork.SetServerAddress(serverHost.c_str(), serverPort);
		g_ENetNetwork.Initialize();
		g_pNetwork = &g_ENetNetwork;
		g_pMockServer = nullptr;
	}
	else
	{
		// Mock mode: local in-process server (default)
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

#if defined(_DEBUG) || defined(DEBUG)

	hal::DebugText dt(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		L"resource/texture/consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(), Direct3D_GetBackBufferHeight(),
		0.0f, 900.0f,
		0, 0,
		0.0f, 16.0f
	);

	Collision_DebugInitialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

#endif // _DEBUG || DEBUG

	//Game_Initialize();

	ShowWindow(hWnd, nCmdShow);

	UpdateWindow(hWnd);

	Mouse_SetVisible(true);



	double exec_last_time = SystemTimer_GetTime();
	double fps_last_time = exec_last_time;
	double current_time = 0.0;
	ULONG frame_count = 0;
	double fps = 0.0;


	MSG msg;


	do{
		if (PeekMessage(&msg,nullptr,0,0,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
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
			//if (elapsed_time >= (1.0 / 60.0)) {  // 60FPSで更新する場合
			if (true){
				exec_last_time = current_time;


				//ゲームループ処理／ゲーム更新
				KeyLogger_Update();
				MSLogger_Update();

				//Game_Update(elapsed_time);
				Scene_Update(elapsed_time);

				// ====================================================================
				// Input Producer: Sample input and send InputCmd to server
				// ====================================================================
				g_InputProducer.Update();

				// ====================================================================
				// Network: Poll events (ENet) or run local server (Mock)
				// ====================================================================
				if (g_NetworkMode == "local" || g_NetworkMode == "remote")
				{
					g_ENetNetwork.PollEvents();
				}
				else
				{
					g_MockServer.Update(elapsed_time);
				}

				Fade_Update(elapsed_time);

				SpriteAnime_Update(elapsed_time);

				//ゲーム描画処理
				Direct3D_Clear();

				Sprite_Begin();

				//Game_Draw();
				Scene_Draw();
				Fade_Draw();





#if defined(_DEBUG) || defined(DEBUG)


#endif // _DEBUG || DEBUG

				Direct3D_Present();

				Scene_Refresh();

				frame_count++;
			}
		}
	} while (msg.message != WM_QUIT);

	//Game_Finalize();

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



	return static_cast<int>(msg.wParam);

}


