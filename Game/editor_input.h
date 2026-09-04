#pragma once
//=============================================================================
// editor_input.h — editor input arbitration (spec 3.6, 8.3).
//
// Split in two halves on purpose:
//
//   editorinput::   PURE logic — dock geometry, the rect hit test, and the
//                   press-latched ownership state machine. No engine headers,
//                   so Game/tests/test_editor_input.cpp compiles it standalone
//                   with no stub TU.
//
//   EditorInput_*   Per-frame glue (editor_input.cpp) that reads MSLogger +
//                   Direct3D and answers "does the viewport own the mouse /
//                   wheel / keys this frame?".
//
// WHY C++ OWNS THE DOCK GEOMETRY: the same numbers drive the C++ hit test and
// (converted to CSS px and pushed over the bridge) the page's CSS variables.
// One owner, so a CSS tweak can never silently desync the arbitration. Never
// hard-code these sizes in editor.css — read the variables.
//=============================================================================

namespace editorinput {

// Docked panel geometry in CLIENT pixels (== back-buffer pixels).
struct DockLayout {
    int topH   = 44;    // toolbar strip across the top
    int rightW = 300;   // inspector column down the right, below the toolbar
};

// Is (x,y) over a docked panel? Client pixels, origin top-left.
// Anything outside the window counts as "not the viewport": a drag that leaves
// the window must not keep picking, and MSLogger can report out-of-range coords.
inline bool HitDock(const DockLayout& d, int x, int y, int screenW, int screenH)
{
    if (x < 0 || y < 0 || x >= screenW || y >= screenH) return true;
    if (y < d.topH) return true;                                  // toolbar spans full width
    if (x >= screenW - d.rightW && y >= d.topH) return true;      // inspector, below the toolbar
    return false;
}

// Who owns the current mouse interaction.
enum class Owner { None, Viewport, UI };

struct Arbiter { Owner owner = Owner::None; };

// Advance one frame.
//   overUI        — HitDock(...) OR a JS-raised pointer capture (floating overlay)
//   anyButtonDown — any mouse button held (or pressed) this frame
//
// Ownership is decided on the PRESS EDGE and held until every button is up.
// That is the whole point: re-deciding per frame would break a gizmo drag the
// moment the cursor crossed the inspector, and would let a slider drag that
// wandered into the viewport start a pick.
inline Owner Step(Arbiter& a, bool overUI, bool anyButtonDown)
{
    if (!anyButtonDown) { a.owner = Owner::None; return Owner::None; }
    if (a.owner == Owner::None) a.owner = overUI ? Owner::UI : Owner::Viewport;
    return a.owner;
}

} // namespace editorinput

// ---- per-frame glue (editor_input.cpp) --------------------------------------

// Call FIRST in SceneEditor_Update, before anything reads the mouse.
void EditorInput_BeginFrame();

// Drop latched ownership and both JS-reported flags. Call on scene enter — a
// stale textFocus==true silently disables every editor hotkey.
void EditorInput_Reset();

bool EditorInput_ViewportOwnsMouse();   // camera / pick / gizmo may run this frame
bool EditorInput_ViewportOwnsWheel();   // dolly (cursor position only — not latched)
bool EditorInput_ViewportOwnsKeys();    // editor hotkeys may run (false while a field has focus)
bool EditorInput_PointerOverUI();       // raw hit result, for the toolbar status readout

// Reported by the page over the bridge (window.editor.setTextFocus / setPointerCapture).
void EditorInput_SetTextFocus(bool on);
void EditorInput_SetPointerCapture(bool on);

const editorinput::DockLayout& EditorInput_Layout();
