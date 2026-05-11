#pragma once
#include <DirectXMath.h>

void Font_Initialize();
void Font_Finalize();
void Font_Draw(const wchar_t* text, float dx, float dy, const DirectX::XMFLOAT4& color);
void Font_Draw(const wchar_t* text, float dx, float dy, const DirectX::XMFLOAT4& color, float scale);

// Word-wrapped text rendering. Text exceeding maxWidth wraps to the next line.
void Font_DrawWrapped(const wchar_t* text, float dx, float dy, float maxWidth, const DirectX::XMFLOAT4& color);

// Pixel width of a single-line string at scale 1.0.
float Font_MeasureWidth(const wchar_t* text);

// Line height in pixels (matches rfont.fnt lineHeight).
float Font_LineHeight();
