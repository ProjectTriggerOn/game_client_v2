#include "ms_logger.h"

#include "direct3d.h"
#include "mouse.h"


namespace 
{
	Mouse_State g_PrevState = {};

	Mouse_State g_TriggerState = {};

	Mouse_State g_ReleaseState = {};

	Mouse_State g_CurrentState = {};

	Mouse_State g_UIPrevState = {};

	Mouse_State g_UITriggerState = {};

	Mouse_State g_UIReleaseState = {};

	Mouse_State g_UICurrentState = {};

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
	LPBYTE pCurrent;
	LPBYTE pPrev;
	LPBYTE pTrigger;
	LPBYTE pRelease;

	if (g_isUIMode) {

		Mouse_GetState(&g_UICurrentState);

		pCurrent = (LPBYTE)&g_UICurrentState;
		pPrev = (LPBYTE)&g_UIPrevState;
		pTrigger = (LPBYTE)&g_UITriggerState;
		pRelease = (LPBYTE)&g_UIReleaseState;
	}
	else {

		Mouse_GetState(&g_CurrentState);

		pCurrent = (LPBYTE)&g_CurrentState;
		pPrev = (LPBYTE)&g_PrevState;
		pTrigger = (LPBYTE)&g_TriggerState;
		pRelease = (LPBYTE)&g_ReleaseState;
	}

	for (int i = 0; i < sizeof(Mouse_State); ++i)
	{
		pTrigger[i] = (pPrev[i] ^ pCurrent[i]) & pCurrent[i];
		pRelease[i] = (pPrev[i] ^ pCurrent[i]) & pPrev[i];
	}
	
	if (g_isUIMode) {
		g_UIPrevState = g_UICurrentState;
	}
	else {
		g_PrevState = g_CurrentState;
	}
}

bool MSLogger_IsPressed(MSLogger_Buttons btn)
{
	return isButtonDown(btn, &g_CurrentState);
}

bool MSLogger_IsTrigger(MSLogger_Buttons btn)
{
	return isButtonDown(btn, &g_TriggerState);
}

bool MSLogger_IsReleased(MSLogger_Buttons btn)
{
	return isButtonDown(btn, &g_ReleaseState);
}

bool MSLogger_IsPressedUI(MSLogger_Buttons btn)
{
	return isButtonDown(btn, &g_UICurrentState);
}

bool MSLogger_IsTriggerUI(MSLogger_Buttons btn)
{
	return isButtonDown(btn, &g_UITriggerState);
}

bool MSLogger_IsReleasedUI(MSLogger_Buttons btn)
{
	return isButtonDown(btn, &g_UIReleaseState);
}

int MSLogger_GetX()
{
	return g_CurrentState.x;
}

int MSLogger_GetXUI()
{
	return g_UICurrentState.x;
}

int MSLogger_GetY()
{
	return g_CurrentState.y;
}

int MSLogger_GetYUI()
{
	return g_UICurrentState.y;
}

int MSLogger_GetScrollWheelValue()
{
	return g_CurrentState.scrollWheelValue;
}

Mouse_PositionMode MSLogger_GetPositionMode()
{
	return g_CurrentState.positionMode;
}

bool isButtonDown(MSLogger_Buttons btn, const Mouse_State* pState)
{
    if (pState == nullptr) {
        return false;
    }
    switch (btn)
    {
    case MBT_LEFT:
        return pState->leftButton;

    case MBT_MIDDLE:
        return pState->middleButton;

    case MBT_RIGHT:
        return pState->rightButton;

    case MBT_X1:
        return pState->xButton1;

    case MBT_X2:
        return pState->xButton2;

	default:
		return false;
    }
}

bool isButtonDown(MSLogger_Buttons btn)
{
	return isButtonDown(btn, &g_CurrentState);
}

void MSLogger_SetUIMode(bool isUIMode)
{
	if (isUIMode) {
		Mouse_SetMode(MOUSE_POSITION_MODE_ABSOLUTE);
	}
	else {
		Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
	}
	g_isUIMode = isUIMode;
}

bool MSLogger_IsUIMode()
{
	return g_isUIMode;	
}



