//#include <Windows.h>
#include <sstream>
#include "debug_ostream.h"
#include "game_window.h"
constexpr char FILE_NAME[] = "tekito.png";

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "direct3d.h"

#include "polygon.h"
#include "shader.h"



//Window procedure prototype claim
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE,_In_ LPSTR, _In_ int nCmdShow)
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	HWND hWnd = GameWindow_Generate(hInstance);

	Direct3D_Initialize(hWnd);

	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	Polygon_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		Direct3D_Clear();

		Polygon_Draw();

		Direct3D_Present();
	}


	Polygon_Finalize();

	Shader_Finalize();

	Direct3D_Finalize();
	return (int)msg.wParam;

}


