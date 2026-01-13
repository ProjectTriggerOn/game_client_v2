#pragma once
#include <cstdint>
#include "mouse.h"
typedef enum MSLogger_Buttons_tag : std::uint8_t
{
	MBT_LEFT,
	MBT_MIDDLE,
	MBT_RIGHT,
	MBT_X1,
	MBT_X2,
} MSLogger_Buttons;

void MSLogger_Initialize(HWND window);

void MSLogger_Finalize();

void MSLogger_Update();

bool MSLogger_IsPressed(MSLogger_Buttons btn);

bool MSLogger_IsTrigger(MSLogger_Buttons btn);

bool MSLogger_IsReleased(MSLogger_Buttons btn);

bool MSLogger_IsPressedUI(MSLogger_Buttons btn);

bool MSLogger_IsTriggerUI(MSLogger_Buttons btn);

bool MSLogger_IsReleasedUI(MSLogger_Buttons btn);

int MSLogger_GetX();

int MSLogger_GetXUI();

int MSLogger_GetY();

int MSLogger_GetYUI();

int MSLogger_GetScrollWheelValue();

Mouse_PositionMode MSLogger_GetPositionMode();

bool isButtonDown(MSLogger_Buttons btn, const Mouse_State* pState);

bool isButtonDown(MSLogger_Buttons btn);

void MSLogger_SetUIMode(bool isUIMode);

bool MSLogger_IsUIMode();






