//=============================================================================
// editor_input.cpp — per-frame half of the editor input arbitration.
// Pure logic + rationale live in editor_input.h.
//=============================================================================
#ifdef EDITOR_ENABLED

#include "editor_input.h"

#include "ms_logger.h"
#include "direct3d.h"

namespace {
    editorinput::DockLayout g_Layout;          // C++ is the single owner of dock geometry
    editorinput::Arbiter    g_Arbiter;
    editorinput::Owner      g_Owner      = editorinput::Owner::None;
    bool                    g_OverUI     = false;
    bool                    g_TextFocus  = false;   // an input in the page has focus
    bool                    g_PtrCapture = false;   // JS raised a floating overlay
}

void EditorInput_BeginFrame()
{
    const int w = (int)Direct3D_GetBackBufferWidth();
    const int h = (int)Direct3D_GetBackBufferHeight();

    // *UI accessors: the editor runs in absolute/UI mouse mode, so the MODE_GAME
    // slot is frozen (see the plan's Global Constraints).
    const int x = MSLogger_GetXUI();
    const int y = MSLogger_GetYUI();

    g_OverUI = editorinput::HitDock(g_Layout, x, y, w, h) || g_PtrCapture;

    // "Down" must include the press edge: a click that begins and ends between
    // two polls would otherwise never latch, and the pick path keys off
    // MSLogger_IsTriggerUI.
    const bool down =
        MSLogger_IsPressedUI(MBT_LEFT)   || MSLogger_IsTriggerUI(MBT_LEFT)   ||
        MSLogger_IsPressedUI(MBT_MIDDLE) || MSLogger_IsTriggerUI(MBT_MIDDLE) ||
        MSLogger_IsPressedUI(MBT_RIGHT)  || MSLogger_IsTriggerUI(MBT_RIGHT);

    g_Owner = editorinput::Step(g_Arbiter, g_OverUI, down);
}

void EditorInput_Reset()
{
    g_Arbiter    = editorinput::Arbiter{};
    g_Owner      = editorinput::Owner::None;
    g_OverUI     = false;
    g_TextFocus  = false;
    g_PtrCapture = false;
}

bool EditorInput_ViewportOwnsMouse()
{
    // Nothing latched yet (hover, or no button seen): fall back to the raw hit so
    // hover-only work (gizmo axis highlight) still gates correctly.
    if (g_Owner == editorinput::Owner::None) return !g_OverUI;
    return g_Owner == editorinput::Owner::Viewport;
}

// The wheel is not part of a press/release interaction, so it is never latched:
// it always follows the cursor. Over a panel the wheel scrolls that panel.
bool EditorInput_ViewportOwnsWheel() { return !g_OverUI; }

// Hotkeys gate on text focus ONLY, not on cursor position — moving the mouse
// over the inspector must not disable Ctrl+Z.
bool EditorInput_ViewportOwnsKeys() { return !g_TextFocus; }

bool EditorInput_PointerOverUI() { return g_OverUI; }

void EditorInput_SetTextFocus(bool on)      { g_TextFocus  = on; }
void EditorInput_SetPointerCapture(bool on) { g_PtrCapture = on; }

const editorinput::DockLayout& EditorInput_Layout() { return g_Layout; }

#endif // EDITOR_ENABLED
