
#include <algorithm>
#include "game_window.h"

#include "keyboard.h"
#include "mouse.h"
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


//Window info
static constexpr char WINDOW_CLASS[] = "GameWindow";//メインウインドウクラス名
static constexpr char TITLE[] = "ウィンドウ表示"; //タイトルバ一のテキスト

HWND GameWindow_Generate(HINSTANCE hInstance)
{
	WNDCLASSEX wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr; // メニューは作らない
	wcex.lpszClassName = WINDOW_CLASS;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);
	RegisterClassEx(&wcex);
	/* メインウインドウの作成 */

	constexpr int SCREEN_WIDTH = 1920;
	constexpr int SCREEN_HEIGHT = 1080;
	DWORD WINDOW_STYLE = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	RECT window_rect{
		0,0,SCREEN_WIDTH,SCREEN_HEIGHT
	};
	AdjustWindowRect(
		&window_rect,
		WINDOW_STYLE,
		FALSE
	);

	const int WINDOW_WIDTH = window_rect.right - window_rect.left;
	const int WINDOW_HEIGHT = window_rect.bottom - window_rect.top;

	//Desktop size

	int desktop_width = GetSystemMetrics(SM_CXSCREEN);
	int desktop_height = GetSystemMetrics(SM_CYSCREEN);

	const int WINDOW_X = std::max((desktop_width - WINDOW_WIDTH) / 2, 0);
	const int WINDOW_Y = std::max((desktop_height - WINDOW_HEIGHT) / 2, 0);

	HWND hWnd = CreateWindow(
		WINDOW_CLASS,
		TITLE,
		WINDOW_STYLE,
		//WS_OVERLAPPEDWINDOW ^WS_THICKFRAME,
		WINDOW_X,
		WINDOW_Y,
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		nullptr,
		nullptr,
		hInstance,
		nullptr);
	return hWnd;
	//	return HWND();
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{

	case WM_ACTIVATEAPP:
		Keyboard_ProcessMessage(message, wParam, lParam);
		Mouse_ProcessMessage(message, wParam, lParam);
		break;
	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
	         Mouse_ProcessMessage(message, wParam, lParam);
	         break;
    case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			SendMessage(hWnd, WM_CLOSE, 0, 0);
		}
     case WM_SYSKEYDOWN:
     case WM_KEYUP:
     case WM_SYSKEYUP:
         Keyboard_ProcessMessage(message, wParam, lParam);
        break;
	case WM_CLOSE:
		if (MessageBox(hWnd, "exit?", "Over", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1) == IDOK) {
			DestroyWindow(hWnd);
		}
		break;
	case WM_DESTROY://ウインドウの破棄メッセ-ジ
		PostQuitMessage(0); //WM_QUITXッセ-ジの送信
		break;
	default:
		//通常のメッセ - ジ処理はこの関数に任せる
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
