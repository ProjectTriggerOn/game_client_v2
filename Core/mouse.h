//--------------------------------------------------------------------------------------
// File: mouse.h
//
// Mouse module
//
//--------------------------------------------------------------------------------------
// 2020/02/11
//    Adapted from DirectXTK and reshaped into a C-style API
//
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#ifndef MOUSE_H
#define MOUSE_H
#pragma once


#include <windows.h>
#include <memory>


// Mouse position mode
typedef enum Mouse_PositionMode_tag
{
    MOUSE_POSITION_MODE_ABSOLUTE, // Absolute coordinates
    MOUSE_POSITION_MODE_RELATIVE, // Relative coordinates
} Mouse_PositionMode;


// Mouse state
typedef struct MouseState_tag
{
    bool leftButton;
    bool middleButton;
    bool rightButton;
    bool xButton1;
    bool xButton2;
    int x;
    int y;
    int scrollWheelValue;
    Mouse_PositionMode positionMode;
} Mouse_State;


// Initialize the mouse module
void Mouse_Initialize(HWND window);

// Shut down the mouse module
void Mouse_Finalize(void);

// Get the mouse state
void Mouse_GetState(Mouse_State* pState);

// Reset the accumulated scroll-wheel value
void Mouse_ResetScrollWheelValue(void);

// Set the position mode (absolute by default)
void Mouse_SetMode(Mouse_PositionMode mode);

// Get current position mode (used by UI input tap to flag relative-mode mouse events)
Mouse_PositionMode Mouse_GetMode(void);

// Is a mouse connected?
bool Mouse_IsConnected(void);

// Is the mouse cursor visible?
bool Mouse_IsVisible(void);

// Show or hide the mouse cursor
void Mouse_SetVisible(bool visible);

// Window-procedure hook that feeds mouse messages to this module
void Mouse_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

// Recompute the cursor clip rect against the current window client area.
// No-op unless the mouse is in RELATIVE mode and the window has focus.
void Mouse_RefreshClipRect(void);


// Usage
//
// Once the target window exists, call the initialize function with its handle
//
// Mouse_Initialize(hwnd);
//
// Call the mouse hook from the window procedure
//
// LResult CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
// {
//     switch (message)
//     {
//     case WM_ACTIVATEAPP:
//     case WM_INPUT:
//     case WM_MOUSEMOVE:
//     case WM_LBUTTONDOWN:
//     case WM_LBUTTONUP:
//     case WM_RBUTTONDOWN:
//     case WM_RBUTTONUP:
//     case WM_MBUTTONDOWN:
//     case WM_MBUTTONUP:
//     case WM_MOUSEWHEEL:
//     case WM_XBUTTONDOWN:
//     case WM_XBUTTONUP:
//     case WM_MOUSEHOVER:
//         Mouse_ProcessMessage(message, wParam, lParam);
//         break;
//
//     }
// }
//

#endif
