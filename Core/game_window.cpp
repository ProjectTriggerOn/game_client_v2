
#include <algorithm>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM (signed; multi-monitor safe)
#include "game_window.h"
#include "config.h"

#include "keyboard.h"
#include "mouse.h"
#include "ui_input_queue.h"
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------------------------
// UI input tap (docs/ultralight_integration.md §5.1)
// Exactly three responsibilities: translate Win32 messages → UIInputEvent,
// attach the current mouse mode, enqueue. No routing decisions, no message
// consumption — KeyLogger/MSLogger keep working untouched.
// ---------------------------------------------------------------------------
static void UITap_ProcessMessage(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	const bool rel = (Mouse_GetMode() == MOUSE_POSITION_MODE_RELATIVE);

	switch (msg)
	{
	case WM_KEYDOWN: case WM_SYSKEYDOWN: {
		UIInputEvent ev{};
		ev.type          = UIInputType::KeyDown;
		ev.is_system_key = (msg == WM_SYSKEYDOWN);
		ev.wparam        = wp;
		ev.lparam        = lp;
		UIInput::Enqueue(ev);
		break;
	}
	case WM_KEYUP: case WM_SYSKEYUP: {
		UIInputEvent ev{};
		ev.type          = UIInputType::KeyUp;
		ev.is_system_key = (msg == WM_SYSKEYUP);
		ev.wparam        = wp;
		ev.lparam        = lp;
		UIInput::Enqueue(ev);
		break;
	}
	case WM_CHAR: {
		UIInputEvent ev{};
		ev.type   = UIInputType::Char;
		ev.wparam = wp;
		ev.lparam = lp;
		UIInput::Enqueue(ev);
		break;
	}
	case WM_MOUSEMOVE: {
		UIInputEvent ev{};
		ev.type              = UIInputType::MouseMove;
		ev.mouse_x           = (int16_t)GET_X_LPARAM(lp);
		ev.mouse_y           = (int16_t)GET_Y_LPARAM(lp);
		ev.mouse_is_relative = rel;
		UIInput::Enqueue(ev);
		break;
	}
	case WM_LBUTTONDOWN: case WM_MBUTTONDOWN: case WM_RBUTTONDOWN: {
		UIInputEvent ev{};
		ev.type              = UIInputType::MouseDown;
		ev.mouse_button      = (msg == WM_LBUTTONDOWN) ? 1u : (msg == WM_MBUTTONDOWN) ? 2u : 3u;
		ev.mouse_x           = (int16_t)GET_X_LPARAM(lp);
		ev.mouse_y           = (int16_t)GET_Y_LPARAM(lp);
		ev.mouse_is_relative = rel;
		UIInput::Enqueue(ev);
		break;
	}
	case WM_LBUTTONUP: case WM_MBUTTONUP: case WM_RBUTTONUP: {
		UIInputEvent ev{};
		ev.type              = UIInputType::MouseUp;
		ev.mouse_button      = (msg == WM_LBUTTONUP) ? 1u : (msg == WM_MBUTTONUP) ? 2u : 3u;
		ev.mouse_x           = (int16_t)GET_X_LPARAM(lp);
		ev.mouse_y           = (int16_t)GET_Y_LPARAM(lp);
		ev.mouse_is_relative = rel;
		UIInput::Enqueue(ev);
		break;
	}
	case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: {
		// Wheel messages carry SCREEN coordinates in lParam (unlike WM_MOUSEMOVE!)
		// — must convert to client area
		POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
		ScreenToClient(hWnd, &pt);
		const int deltaPx = MulDiv(GET_WHEEL_DELTA_WPARAM(wp), 100, WHEEL_DELTA); // ≈100px per notch
		UIInputEvent ev{};
		ev.type              = UIInputType::Scroll;
		ev.mouse_x           = (int16_t)pt.x;
		ev.mouse_y           = (int16_t)pt.y;
		ev.mouse_is_relative = rel;
		if (msg == WM_MOUSEWHEEL) ev.scroll_delta_y = (int16_t)deltaPx;
		else                      ev.scroll_delta_x = (int16_t)deltaPx;
		UIInput::Enqueue(ev);
		break;
	}
	default:
		break;
	}
}


//Window info
// Unicode (W-API) registration: an ANSI window's WM_CHAR can only carry bytes
// of the current code page — non-ASCII input turns into '?'. The UI text input
// needs UTF-16 WM_CHAR (docs §5.1).
// Note: a Unicode window must be paired with DefWindowProcW (see end of WndProc).
static constexpr wchar_t WINDOW_CLASS[] = L"GameWindow";//メインウインドウクラス名
static constexpr wchar_t TITLE[] = L"Trigger On"; //タイトルバーのテキスト

HWND GameWindow_Generate(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr; // メニューは作らない
	wcex.lpszClassName = WINDOW_CLASS;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);
	RegisterClassExW(&wcex);
	/* メインウインドウの作成 */

	const int SCREEN_WIDTH  = Config::GetInstance().GetInt("client", "window_width",  1920);
	const int SCREEN_HEIGHT = Config::GetInstance().GetInt("client", "window_height", 1080);
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

	HWND hWnd = CreateWindowW(
		WINDOW_CLASS,
		TITLE,
		WINDOW_STYLE,
		WINDOW_X,
		WINDOW_Y,
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		nullptr,
		nullptr,
		hInstance,
		nullptr);
	return hWnd;
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// UI tap: enqueue first (non-consuming), then fall through to existing routing
	UITap_ProcessMessage(hWnd, message, wParam, lParam);

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
	case WM_WINDOWPOSCHANGED:
	case WM_DISPLAYCHANGE:
		Mouse_RefreshClipRect();
		return DefWindowProcW(hWnd, message, wParam, lParam);
    case WM_KEYDOWN:
        // ESC is handled by the game loop (toggles settings menu)
     case WM_SYSKEYDOWN:
     case WM_KEYUP:
     case WM_SYSKEYUP:
         Keyboard_ProcessMessage(message, wParam, lParam);
        break;
	case WM_CLOSE:
		Mouse_SetMode(MOUSE_POSITION_MODE_ABSOLUTE);
		Mouse_SetVisible(true);
		if (MessageBoxW(hWnd, L"exit?", L"Over", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1) == IDOK) {
			DestroyWindow(hWnd);
		}
		break;
	case WM_DESTROY://ウインドウの破棄メッセ-ジ
		PostQuitMessage(0); //WM_QUITXッセ-ジの送信
		break;
	default:
		//通常のメッセ - ジ処理はこの関数に任せる
		// A Unicode window (RegisterClassExW) must use the W variant,
		// or character messages get mis-converted
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}
