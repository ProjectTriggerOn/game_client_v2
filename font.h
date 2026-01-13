#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

// --- 现有函数 (保持不变) ---
void Font_Initialize();
void Font_Finalize();
void Font_Draw(const wchar_t* text, float dx, float dy, const DirectX::XMFLOAT4& color);

// --- 【新增】支持自动换行的文本绘制函数 ---
// text: 要绘制的文本
// dx, dy: 绘制区域的左上角坐标
// maxWidth: 文本区域的最大宽度，超过此宽度将自动换行
// color: 文本颜色
void Font_DrawWrapped(const wchar_t* text, float dx, float dy, float maxWidth, const DirectX::XMFLOAT4& color);
