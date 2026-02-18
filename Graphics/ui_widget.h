#pragma once
//=============================================================================
// ui_widget.h
//
// Immediate-mode 2D UI widgets built on Sprite_Draw + Font_Draw.
// Uses frame.png as the panel/button background (semi-transparent rounded rect).
//
// Usage:
//   Widget_Initialize();             // call once at startup
//   Widget_Finalize();               // call once at shutdown
//
//   // Each frame (after Sprite_Begin()):
//   Widget_DrawPanel(x, y, w, h, L"HP: 200");
//   if (Widget_DrawButton(x, y, w, h, L"Start")) { ... }
//=============================================================================
#include <DirectXMath.h>

//-----------------------------------------------------------------------------
// Lifecycle
//-----------------------------------------------------------------------------
void Widget_Initialize();
void Widget_Finalize();

//-----------------------------------------------------------------------------
// Panel — background frame + optional centered text (HUD display)
//
//  x, y       : top-left screen position (pixels)
//  w, h       : size in pixels
//  text       : UTF-16 string, nullptr for background-only
//  bgColor    : RGBA tint applied to frame.png
//  textColor  : text color
//  textScale  : uniform scale for the text (1.0 = native 32px)
//-----------------------------------------------------------------------------
void Widget_DrawPanel(
    float x, float y, float w, float h,
    const wchar_t*               text      = nullptr,
    const DirectX::XMFLOAT4&    bgColor   = { 0.10f, 0.10f, 0.15f, 0.75f },
    const DirectX::XMFLOAT4&    textColor = { 1.0f,  1.0f,  1.0f,  1.0f  },
    float                        textScale = 1.0f);

//-----------------------------------------------------------------------------
// Button — clickable panel; returns true the frame the user clicks it
//
//  Normal   : dark blue-gray
//  Hovered  : brighter blue
//  Pressed  : deep blue (held)
//  Returns  : true on the frame MBT_LEFT is triggered inside the button rect
//-----------------------------------------------------------------------------
bool Widget_DrawButton(
    float x, float y, float w, float h,
    const wchar_t* label,
    float          textScale = 1.0f);

//-----------------------------------------------------------------------------
// Slider — horizontal drag control; returns the (possibly updated) value
//
//  value    : current value (caller owns and stores this)
//  minVal   : minimum value
//  maxVal   : maximum value
//  label    : optional text centered over the slider (e.g. L"Volume")
//
// Layout:  [========O---------]
//           filled   thumb
//
// The thumb is a square of height h; the track is h/4 thick.
// Clicking anywhere on the widget begins dragging; releasing ends it.
//-----------------------------------------------------------------------------
float Widget_DrawSlider(
    float x, float y, float w, float h,
    float value, float minVal, float maxVal,
    const wchar_t* label = nullptr);
