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

#include "texture.h"



//Window procedure prototype claim
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE,_In_ LPSTR, _In_ int nCmdShow)
{
	(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	HWND hWnd = GameWindow_Generate(hInstance);

	Direct3D_Initialize(hWnd);

	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Sprite_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Texture_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	int texid_image = Texture_LoadFromFile(L"image.png");
	int texid_knight= Texture_LoadFromFile(L"knight.png");


	ShowWindow(hWnd, nCmdShow);

	UpdateWindow(hWnd);

	MSG msg;

	float x = 0.0f;

	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		Direct3D_Clear();

		//Sprite_Draw(32.0f,32.0f);
//		for (int i=0;i<4;i++)
//		{
//			float sx = 32.0f + i * 100.0f;
//			float sy = 32.0f;
//			Sprite_Draw(sx, sy);
//		}
		Texture_Set(texid_image);
		Sprite_Draw(x, 32.0f,600.0f,600.0f);
		Texture_Set(texid_knight);
		Sprite_Draw(32.0f, 32.0f, 600.0f, 600.0f);
		x += 0.3f;

		Direct3D_Present();
	}


	Sprite_Finalize();

	Texture_AllRelease();

	Shader_Finalize();

	Direct3D_Finalize();
	return (int)msg.wParam;

}


