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
#include "sprite_anime.h"

#include "debug_text.h"

#include "texture.h"

#include <chrono>
#include <thread>

#include "system_timer.h"

#include <sstream>

#include "polygon.h"

#include "DirectXMath.h"
#include "key_logger.h"
#include "mouse.h"

#include <Xinput.h>
#pragma comment(lib, "xinput.lib")


//Window procedure prototype claim
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE,_In_ LPSTR, _In_ int nCmdShow)
{
	(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	HWND hWnd = GameWindow_Generate(hInstance);

	SystemTimer_Initialize();

	Direct3D_Initialize(hWnd);

	KeyLogger_Initialize();

	Mouse_Initialize(hWnd);

	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Texture_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Polygon_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Sprite_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	SpriteAnime_Initialize();

	hal::DebugText dt(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		L"resource/texture/consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(),Direct3D_GetBackBufferHeight(),
		0.0f,0.0f,
		0,0,
		0.0f,0.0f
	);


	int texid_image = Texture_LoadFromFile(L"resource/texture/image.png");
	int texid_knight= Texture_LoadFromFile(L"resource/texture/knight.png");
	int texid_sozai = Texture_LoadFromFile(L"resource/texture/kokosozai.png");
	int texid_runningman_01 = Texture_LoadFromFile(L"resource/texture/runningman001.png");
	int texid_black = Texture_LoadFromFile(L"resource/texture/black.png");

	int aid_rw = SpriteAnime_PatternRegister(texid_sozai, 13,0.1, { 32,32 }, { 0,0 },true,13);
	int aid_lw = SpriteAnime_PatternRegister(texid_sozai, 13, 0.1, { 32,32 }, { 0,32 }, true, 13);
	int aid_bw = SpriteAnime_PatternRegister(texid_sozai, 6, 0.1,{ 32,32 }, { 0,32*2 },true, 6);
	int aid_tc = SpriteAnime_PatternRegister(texid_sozai, 4,0.1, { 32,32 }, { 32*2,32*5 },true, 4);
	int aid_idle = SpriteAnime_PatternRegister(texid_sozai, 15,0.1, { 32,32 }, { 32*0,32*4 },true, 15);
	int aid_hd = SpriteAnime_PatternRegister(texid_runningman_01, 10, 0.2, { 140,200 }, {0,0},true,5);

	int pid01 = SpriteAnime_CreatePlayer(aid_rw);
	int pid02 = SpriteAnime_CreatePlayer(aid_lw);
	int pid03 = SpriteAnime_CreatePlayer(aid_idle);
	int pid04 = SpriteAnime_CreatePlayer(aid_hd);
	ShowWindow(hWnd, nCmdShow);

	UpdateWindow(hWnd);

	//ULONGLONG frame_count = 0;
	//double register_time = SystemTimer_GetTime();
	//double register_time_m = SystemTimer_GetTime();
	//double fps = 0.0;
	
	double exec_last_time = SystemTimer_GetTime();
	double fps_last_time = exec_last_time;
	double current_time = 0.0;
	ULONG frame_count = 0;
	double fps = 0.0;
	double angle = 0.0f;
	float angle2 = 0.0f;
	float x = 0.0f;
	float y = 0.0f;
	int texid = pid01;
	Mouse_SetVisible(false);

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
			
			//double now  = SystemTimer_GetTime();
			//double elapsed_time = now - register_time;
			//register_time = now;

			//double elapsed_time_m = now - register_time_m;
			
			if (elapsed_time >= 1.0)
			{
				fps = frame_count / elapsed_time;
				fps_last_time = current_time;
				frame_count = 0;
			}

			elapsed_time = current_time - exec_last_time;
			//if (elapsed_time < 0.001) {
			if (true){
				exec_last_time = current_time;


				//ゲームループ処理／ゲーム更新
				KeyLogger_Update();
				Mouse_State ms{};
				Mouse_GetState(&ms);
				SpriteAnime_Update(elapsed_time);


				//ゲーム描画処理
				Direct3D_Clear();

				Sprite_Begin();

				float ky = sinf(angle) * 50.0f + 64.0f;
				angle += 6.0f * elapsed_time;
				Sprite_Draw(texid_knight, 256.0f, ky, 256.0f, 256.0f, 0, 0, 1024, 1024);
				
				angle2 += DirectX::XM_2PI * elapsed_time;
				//Sprite_Draw(texid_sozai, x, y, 256.0f, 256.0f, 0, 0, 32, 32,angle2);

				SpriteAnime_Draw(texid, x, y, 300, 300);

				XINPUT_STATE xs{};
				XINPUT_KEYSTROKE xks{};

				XInputGetState(0, &xs);
				XInputGetKeystroke(0, 0, &xks);

				XINPUT_VIBRATION xv{};

				float Speed = 200.0f;

				


				float delta_x = (float)xs.Gamepad.sThumbLX * 0.01f * elapsed_time;

				if (delta_x >=0)
				{
					x += delta_x ;
					texid = pid01; // 右
				}
				else
				{
					x += delta_x;
					texid = pid02; // 左
				}
				
				
				
				

				y -= (float)xs.Gamepad.sThumbLY * 0.01f * elapsed_time;


				if (xs.Gamepad.wButtons & XINPUT_GAMEPAD_A)
				{
					xv.wLeftMotorSpeed = 65535;
					xv.wRightMotorSpeed = 65535;
					XInputSetState(0, &xv);
				}
				else
				{
					xv.wLeftMotorSpeed = 0;
					xv.wRightMotorSpeed = 0;
					XInputSetState(0, &xv);
				}


				if (KeyLogger_IsPressed(KK_D))
				{
					x += static_cast<float>(Speed * elapsed_time);
				}if (KeyLogger_IsTrigger(KK_W))
				{
					y -= static_cast<float>(Speed * elapsed_time);
				}
				if (KeyLogger_IsReleased(KK_A))
				{
					x -= static_cast<float>(Speed * elapsed_time);
				}
				if (KeyLogger_IsPressed(KK_S))
				{
					y += static_cast<float>(Speed * elapsed_time);
				}

				//Polygon_Draw();

				SpriteAnime_Draw(pid01, 400, 32.0,300,300);
				SpriteAnime_Draw(pid02, 700, 32.0, 300, 300);

				SpriteAnime_Draw(pid03, ms.x, ms.y, 300, 300);
				SpriteAnime_Draw(pid04, 400, 364, 280, 400);
				Texture_Set(texid_black);
				Polygon_Draw();

#if defined(_DEBUG) || defined(DEBUG)
				std::stringstream ss;
				std::stringstream ssf;

				ss << "Frame " << frame_count << "\n";
				ssf << "FPS: " << fps << "\n";
				dt.SetText(ss.str().c_str(), { 1.0f, 1.0f, 1.0f, 1.0f });
				dt.SetText(ssf.str().c_str(), { 1.0f, 1.0f, 1.0f, 1.0f });
				dt.SetText("Hello, World!\n");
				dt.SetText("Direct3D 11 with C++\n");
				dt.SetText("Sprite and Texture Example\n");
				dt.SetText("Debug Text Example\n");
				//dt.SetText("Press any key to continue...");

				dt.Draw();
				dt.Clear();
#endif // _DEBUG || DEBUG
				//std::this_thread::sleep_for(std::chrono::milliseconds(100));

				//Polygon_Draw();

				Direct3D_Present();

				frame_count++;
			}

			
		}
	} while (msg.message != WM_QUIT);


	SpriteAnime_Finalize();

	Texture_Finalize();

	Sprite_Finalize();

	Texture_AllRelease();

	Shader_Finalize();

	Polygon_Finalize();

	Direct3D_Finalize();

	Mouse_Finalize();

	return (int)msg.wParam;

}


