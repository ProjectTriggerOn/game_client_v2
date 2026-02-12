#include "key_logger.h"

static Keyboard_State g_PrevState{};

static Keyboard_State g_TriggerState{};

static Keyboard_State g_ReleaseState{};


void KeyLogger_Initialize(void)
{
	Keyboard_Initialize();
}

void KeyLogger_Update()
{
	const Keyboard_State* currentState = Keyboard_GetState();
	LPBYTE pCurrent = (LPBYTE)currentState;
	LPBYTE pPrev = (LPBYTE)&g_PrevState;
	LPBYTE pTrigger = (LPBYTE)&g_TriggerState;
	LPBYTE pRelease = (LPBYTE)&g_ReleaseState;
	//0 1 -> 1
	//1 0 -> 0
	//1 1 -> 0
	//0 0 -> 0
	for (int i = 0; i < sizeof(Keyboard_State); ++i)
	{
		pTrigger[i] = (pPrev[i] ^ pCurrent[i]) & pCurrent[i];
		pRelease[i] = (pPrev[i] ^ pCurrent[i]) & pPrev[i];

	}

	g_PrevState = *currentState;
}

bool KeyLogger_IsPressed(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key);
}

bool KeyLogger_IsTrigger(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key,&g_TriggerState);
}

bool KeyLogger_IsReleased(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key, &g_ReleaseState);
}

