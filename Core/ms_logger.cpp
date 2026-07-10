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
	Mouse_State sampled = {};
	Mouse_GetState(&sampled);

	// Route by the mode the sample was ACTUALLY taken in, not by g_isUIMode.
	//
	// Mouse_SetMode only signals an event; the hardware switch happens inside
	// Mouse_ProcessMessage when the next MOUSE message arrives. Toggling modes
	// from a keyboard event (ESC) therefore leaves a multi-frame window where
	// the sample still carries ABSOLUTE pixel coordinates — if those were
	// written into MODE_GAME, PlayerCamFps would consume e.g. (960, 540) as a
	// mouse delta and slam the pitch straight into the ground.
	const bool isRelative = (sampled.positionMode == MOUSE_POSITION_MODE_RELATIVE);

	ModeState& m = g_Mode[isRelative ? MODE_GAME : MODE_UI];
	m.current = sampled;

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

	// Neutralize the inactive slot's motion values: while in absolute mode the
	// GAME slot must read as "no movement" (deltas), not stale/foreign data.
	// (The UI slot keeps its last absolute position — frozen cursor is correct.)
	if (!isRelative)
	{
		g_Mode[MODE_GAME].current.x = 0;
		g_Mode[MODE_GAME].current.y = 0;
	}
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

int MSLogger_GetScrollWheelValueUI()
{
	// In absolute/UI mouse mode the sampler writes wheel state into MODE_UI, so
	// callers running in that mode (e.g. SCENE_EDITOR) must read this slot — the
	// MODE_GAME wheel value is frozen there. Mirrors the *UI x/y accessors.
	return g_Mode[MODE_UI].current.scrollWheelValue;
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
	// Only flips MSLogger's coordinate-source flag. The hardware cursor mode is
	// owned by MousePolicy_Apply (Game/mouse_policy.h), which also calls this —
	// keeping both in sync from a single place.
	g_isUIMode = isUIMode;
}

bool MSLogger_IsUIMode()
{
	return g_isUIMode;
}
