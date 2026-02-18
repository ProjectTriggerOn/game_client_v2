#include "ui_widget.h"
#include "sprite.h"
#include "texture.h"
#include "font.h"
#include "ms_logger.h"

using namespace DirectX;

namespace
{
    int g_FrameTexId = -1;
    int g_WhiteTexId = -1;

    // Button / thumb state palette
    constexpr XMFLOAT4 BTN_NORMAL  = { 0.12f, 0.14f, 0.22f, 0.82f };
    constexpr XMFLOAT4 BTN_HOVERED = { 0.28f, 0.40f, 0.62f, 0.90f };
    constexpr XMFLOAT4 BTN_PRESSED = { 0.07f, 0.18f, 0.38f, 0.96f };
    constexpr XMFLOAT4 BTN_TEXT    = { 1.0f,  1.0f,  1.0f,  1.0f  };

    // Slider track palette (solid colors)
    constexpr XMFLOAT4 TRACK_BG    = { 0.20f, 0.20f, 0.28f, 1.00f };
    constexpr XMFLOAT4 TRACK_FILL  = { 0.25f, 0.60f, 1.00f, 1.00f };
    constexpr XMFLOAT4 THUMB_NORMAL  = { 0.70f, 0.70f, 0.80f, 1.00f };
    constexpr XMFLOAT4 THUMB_HOVERED = { 1.00f, 1.00f, 1.00f, 1.00f };
    constexpr XMFLOAT4 THUMB_PRESSED = { 0.40f, 0.75f, 1.00f, 1.00f };

    // Slider drag state — tracks which slider widget is being dragged
    // using its (x,y) position as a lightweight identity
    float s_dragSliderX = -1.0f;
    float s_dragSliderY = -1.0f;
}

//-----------------------------------------------------------------------------
void Widget_Initialize()
{
    g_FrameTexId = Texture_LoadFromFile(L"resource/texture/frame.png");
    g_WhiteTexId = Texture_LoadFromFile(L"resource/texture/white.png");
}

void Widget_Finalize()
{
    // Texture lifetime is managed by the Texture subsystem
    g_FrameTexId = -1;
    g_WhiteTexId = -1;
}

//-----------------------------------------------------------------------------
// Internal helper — draw frame background (uses frame.png)
//-----------------------------------------------------------------------------
static void DrawFrame(float x, float y, float w, float h, const XMFLOAT4& color)
{
    if (g_FrameTexId >= 0)
        Sprite_Draw(g_FrameTexId, x, y, w, h, color);
}

//-----------------------------------------------------------------------------
// Internal helper — draw solid color rectangle (uses white.png)
//-----------------------------------------------------------------------------
static void DrawRect(float x, float y, float w, float h, const XMFLOAT4& color)
{
    if (g_WhiteTexId >= 0)
        Sprite_Draw(g_WhiteTexId, x, y, w, h, color);
}

//-----------------------------------------------------------------------------
// Internal helper — draw horizontally + vertically centered text inside a rect
//-----------------------------------------------------------------------------
static void DrawCenteredText(float x, float y, float w, float h,
                             const wchar_t* text,
                             const XMFLOAT4& color, float scale)
{
    if (!text || text[0] == L'\0') return;

    float textW = Font_MeasureWidth(text) * scale;
    float lineH = Font_LineHeight() * scale;

    float tx = x + (w - textW) * 0.5f;
    float ty = y + (h - lineH) * 0.5f;

    if (scale == 1.0f)
        Font_Draw(text, tx, ty, color);
    else
        Font_Draw(text, tx, ty, color, scale);
}

//-----------------------------------------------------------------------------
void Widget_DrawPanel(float x, float y, float w, float h,
                      const wchar_t* text,
                      const XMFLOAT4& bgColor,
                      const XMFLOAT4& textColor,
                      float textScale)
{
    DrawFrame(x, y, w, h, bgColor);
    DrawCenteredText(x, y, w, h, text, textColor, textScale);
}

//-----------------------------------------------------------------------------
bool Widget_DrawButton(float x, float y, float w, float h,
                       const wchar_t* label,
                       float textScale)
{
    int mx = MSLogger_GetXUI();
    int my = MSLogger_GetYUI();

    bool hovered = (mx >= (int)x && mx < (int)(x + w) &&
                    my >= (int)y && my < (int)(y + h));
    bool pressed  = hovered && MSLogger_IsPressedUI(MBT_LEFT);
    bool clicked  = hovered && MSLogger_IsReleasedUI(MBT_LEFT);

    const XMFLOAT4& bg = pressed  ? BTN_PRESSED
                       : hovered  ? BTN_HOVERED
                                  : BTN_NORMAL;

    DrawFrame(x, y, w, h, bg);
    DrawCenteredText(x, y, w, h, label, BTN_TEXT, textScale);

    return clicked;
}

//-----------------------------------------------------------------------------
float Widget_DrawSlider(float x, float y, float w, float h,
                        float value, float minVal, float maxVal,
                        const wchar_t* label)
{
    // Clamp incoming value
    if (value < minVal) value = minVal;
    if (value > maxVal) value = maxVal;

    // Geometry — track occupies full height, thumb is a square
    const float thumbW   = h;
    const float trackX0  = x + thumbW * 0.5f;
    const float trackLen = w - thumbW;
    const float trackH   = h;           // full height, solid bar
    const float trackY   = y;

    // Normalised position [0..1]
    float t = (maxVal > minVal) ? (value - minVal) / (maxVal - minVal) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float thumbCX = trackX0 + trackLen * t;
    float thumbX  = thumbCX - thumbW * 0.5f;

    // ---- Mouse interaction ------------------------------------------------
    int mx = MSLogger_GetXUI();
    int my = MSLogger_GetYUI();

    bool inWidget = (mx >= (int)x && mx < (int)(x + w) &&
                     my >= (int)y && my < (int)(y + h));

    bool isMe = (s_dragSliderX == x && s_dragSliderY == y);

    // Acquire drag
    if (MSLogger_IsTriggerUI(MBT_LEFT) && inWidget && s_dragSliderX < 0.0f)
    {
        s_dragSliderX = x;
        s_dragSliderY = y;
        isMe = true;
    }
    // Release drag
    if (MSLogger_IsReleasedUI(MBT_LEFT) && isMe)
    {
        s_dragSliderX = -1.0f;
        s_dragSliderY = -1.0f;
        isMe = false;
    }

    // Update value while dragging
    if (isMe && MSLogger_IsPressedUI(MBT_LEFT))
    {
        float relX = (float)mx - trackX0;
        t = relX / trackLen;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        value   = minVal + t * (maxVal - minVal);
        thumbCX = trackX0 + trackLen * t;
        thumbX  = thumbCX - thumbW * 0.5f;
    }

    // ---- Draw (solid color, no frame.png) ---------------------------------

    // Track background
    DrawRect(x, trackY, w, trackH, TRACK_BG);

    // Filled portion (left → thumb centre)
    float fillW = thumbCX - x;
    if (fillW > 0.0f)
        DrawRect(x, trackY, fillW, trackH, TRACK_FILL);

    // Thumb — tall narrow rectangle
    bool thumbHovered = !isMe && (mx >= (int)thumbX && mx < (int)(thumbX + thumbW) &&
                                  my >= (int)y       && my < (int)(y + h));
    const XMFLOAT4& thumbColor = isMe          ? THUMB_PRESSED
                                : thumbHovered  ? THUMB_HOVERED
                                               : THUMB_NORMAL;
    DrawRect(thumbX, y, thumbW, h, thumbColor);

    // Label centered over the whole widget
    if (label && label[0] != L'\0')
        DrawCenteredText(x, y, w, h, label, { 0.05f, 0.05f, 0.05f, 1.0f }, 1.0f);

    return value;
}
