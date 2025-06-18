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


//Window procedure prototype claim
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE,_In_ LPSTR, _In_ int nCmdShow)
{
	(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	HWND hWnd = GameWindow_Generate(hInstance);

	SystemTimer_Initialize();

	Direct3D_Initialize(hWnd);

	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Texture_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());


	Sprite_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	SpriteAnime_Initialize();

	hal::DebugText dt(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		L"consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(),Direct3D_GetBackBufferHeight(),
		0.0f,0.0f,
		0,0,
		0.0f,0.0f
	);








	int texid_image = Texture_LoadFromFile(L"image.png");
	int texid_knight= Texture_LoadFromFile(L"knight.png");
	int texid_sozai = Texture_LoadFromFile(L"kokosozai.png");


	ShowWindow(hWnd, nCmdShow);

	UpdateWindow(hWnd);

	ULONGLONG frame_count = 0;
	double register_time = SystemTimer_GetTime();
	double register_time_m = SystemTimer_GetTime();
	double fps = 0.0;

	MSG msg;

	float x = 0.0f;
	int c = 0.0f;

	do{
		if (PeekMessage(&msg,nullptr,0,0,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {

			double now  = SystemTimer_GetTime();
			double elapsed_time = now - register_time;
			register_time = now;

			double elapsed_time_m = now - register_time_m;
			
			if (elapsed_time_m >= 1.0)
			{
				fps = frame_count / elapsed_time_m;
				frame_count = 0;
				register_time_m = now;
			}
			

			//ゲームループ処理／ゲーム更新
			SpriteAnime_Update(elapsed_time);
			//ゲーム描画処理
			Direct3D_Clear();

			Sprite_Begin();

			static float b = 0.0f;
			if (c % 1000==0)
			{
				b = b == 0.0f ? 1.0f : 0.0f; 
			}

			//Texture_Set(texid_image);
			//Sprite_Draw(texid_image, 0.0f, 0.0f,900,900);
			//Sprite_Draw(texid_knight, 150.0f, 150.0f, 600,600);
			//Sprite_Draw(texid_image,0.0f, 0.0f,{b,b,0.0f,1.0f});

			//Texture_Set(texid_knight);
			//Sprite_Draw(32.0f, 32.0f, 600.0f, 600.0f);

			Texture_Set(texid_sozai);
			//Sprite_Draw(texid_sozai, 700, 64.0f,450,900 ,32 * 2, 32 * 3, 32, 64);
			//Sprite_Draw(800.0f, 60.0f, 900.0f, 900.0f);

			SpriteAnime_Draw(0,100,32.0,300,300);
			SpriteAnime_Draw(1, 400, 32.0, 300, 300);
			SpriteAnime_Draw(2, 700, 32.0, 300, 300);
			SpriteAnime_Draw(3, 100, 332, 300, 300);
			SpriteAnime_Draw(4, 400, 332, 300, 300);
			SpriteAnime_Draw(5, 700, 332, 300, 300);
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

			Direct3D_Present();

			frame_count++;
		}
	} while (msg.message != WM_QUIT);


	SpriteAnime_Finalize();

	Texture_Finalize();

	Sprite_Finalize();

	Texture_AllRelease();

	Shader_Finalize();

	Direct3D_Finalize();
	return (int)msg.wParam;

}


