#include "ms_logger.h"

#include "mouse.h"


namespace 
{
	Mouse_State g_PrevState = {};

	Mouse_State g_TriggerState = {};

	Mouse_State g_ReleaseState = {};

	Mouse_State g_CurrentState = {};
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
	Mouse_GetState(&g_CurrentState);

	LPBYTE pCurrent = (LPBYTE)&g_CurrentState;
	LPBYTE pPrev = (LPBYTE)&g_PrevState;
	LPBYTE pTrigger = (LPBYTE)&g_TriggerState;
	LPBYTE pRelease = (LPBYTE)&g_ReleaseState;

	for (int i = 0; i < sizeof(Mouse_State); ++i)
	{
		pTrigger[i] = (pPrev[i] ^ pCurrent[i]) & pCurrent[i];
		pRelease[i] = (pPrev[i] ^ pCurrent[i]) & pPrev[i];
	}

	g_PrevState = g_CurrentState;
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

int MSLogger_GetX()
{
	return g_CurrentState.x;
}

int MSLogger_GetY()
{
	return g_CurrentState.y;
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


