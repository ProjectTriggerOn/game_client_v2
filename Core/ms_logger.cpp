#include "ms_logger.h"

#include "direct3d.h"
#include "mouse.h"

namespace
{
	struct ModeState
	{
		Mouse_State current = {};
		Mouse_State prev = {};
		Mouse_State trigger = {};
		Mouse_State release = {};
	};

	enum { MODE_GAME = 0, MODE_UI = 1 };

	ModeState g_Mode[2];
	bool g_isUIMode = false;
}

void MSLogger_Initialize(HWND window)
{
	Mouse_Initialize(window);
}

void MSLogger_Finalize()
{
}

void MSLogger_Update()
{
	ModeState& m = g_Mode[g_isUIMode ? MODE_UI : MODE_GAME];

	Mouse_GetState(&m.current);

	LPBYTE pCurrent = (LPBYTE)&m.current;
	LPBYTE pPrev    = (LPBYTE)&m.prev;
	LPBYTE pTrigger = (LPBYTE)&m.trigger;
	LPBYTE pRelease = (LPBYTE)&m.release;

	for (int i = 0; i < sizeof(Mouse_State); ++i)
	{
		pTrigger[i] = (pPrev[i] ^ pCurrent[i]) & pCurrent[i];
		pRelease[i] = (pPrev[i] ^ pCurrent[i]) & pPrev[i];
	}

	m.prev = m.current;
}

bool MSLogger_IsPressed(MSLogger_Buttons btn)    { return isButtonDown(btn, &g_Mode[MODE_GAME].current); }
bool MSLogger_IsTrigger(MSLogger_Buttons btn)    { return isButtonDown(btn, &g_Mode[MODE_GAME].trigger); }
bool MSLogger_IsReleased(MSLogger_Buttons btn)   { return isButtonDown(btn, &g_Mode[MODE_GAME].release); }

bool MSLogger_IsPressedUI(MSLogger_Buttons btn)  { return isButtonDown(btn, &g_Mode[MODE_UI].current); }
bool MSLogger_IsTriggerUI(MSLogger_Buttons btn)  { return isButtonDown(btn, &g_Mode[MODE_UI].trigger); }
bool MSLogger_IsReleasedUI(MSLogger_Buttons btn) { return isButtonDown(btn, &g_Mode[MODE_UI].release); }

int MSLogger_GetX()    { return g_Mode[MODE_GAME].current.x; }
int MSLogger_GetXUI()  { return g_Mode[MODE_UI].current.x; }
int MSLogger_GetY()    { return g_Mode[MODE_GAME].current.y; }
int MSLogger_GetYUI()  { return g_Mode[MODE_UI].current.y; }

int MSLogger_GetScrollWheelValue()
{
	return g_Mode[MODE_GAME].current.scrollWheelValue;
}

Mouse_PositionMode MSLogger_GetPositionMode()
{
	return g_Mode[MODE_GAME].current.positionMode;
}

bool isButtonDown(MSLogger_Buttons btn, const Mouse_State* pState)
{
	if (!pState) return false;
	switch (btn)
	{
	case MBT_LEFT:   return pState->leftButton;
	case MBT_MIDDLE: return pState->middleButton;
	case MBT_RIGHT:  return pState->rightButton;
	case MBT_X1:     return pState->xButton1;
	case MBT_X2:     return pState->xButton2;
	default:         return false;
	}
}

bool isButtonDown(MSLogger_Buttons btn)
{
	return isButtonDown(btn, &g_Mode[MODE_GAME].current);
}

void MSLogger_SetUIMode(bool isUIMode)
{
	Mouse_SetMode(isUIMode ? MOUSE_POSITION_MODE_ABSOLUTE : MOUSE_POSITION_MODE_RELATIVE);
	g_isUIMode = isUIMode;
}

bool MSLogger_IsUIMode()
{
	return g_isUIMode;
}
