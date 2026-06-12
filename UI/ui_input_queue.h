#pragma once
#include <cstdint>

// UI input event queue: WndProc (producer) → UI::ProcessInput (consumer).
// Both ends live on the main thread (message pump and frame loop share it),
// but the queue is written as SPSC anyway to be future-proof.
// Design per docs/ultralight_integration.md §4.2 / §5.1.

enum class UIInputType : uint8_t {
    KeyDown,    // WM_KEYDOWN / WM_SYSKEYDOWN
    KeyUp,      // WM_KEYUP / WM_SYSKEYUP
    Char,       // WM_CHAR
    MouseMove,
    MouseDown,
    MouseUp,
    Scroll,     // WM_MOUSEWHEEL / WM_MOUSEHWHEEL (coordinates already client-area)
};

struct UIInputEvent {
    UIInputType type;
    bool        is_system_key;      // true for WM_SYSKEY* (Ultralight KeyEvent needs it)
    bool        mouse_is_relative;  // mouse mode at enqueue time; consumer drops RELATIVE MouseMoves
    uint8_t     mouse_button;       // maps to ultralight::MouseEvent::Button (1=L 2=M 3=R)

    // Key events keep the raw Win32 params so KeyEvent's Win32 constructor can
    // consume them directly (no hand-written VK→GK mapping needed)
    uintptr_t   wparam;
    intptr_t    lparam;

    int16_t     mouse_x;
    int16_t     mouse_y;
    int16_t     scroll_delta_x;     // pixels
    int16_t     scroll_delta_y;     // pixels
};

namespace UIInput {

// Called by WndProc. When full, drops the event and bumps a counter
// (256 capacity per frame — never hit in practice).
void Enqueue(const UIInputEvent& ev);

// Called by UI::ProcessInput. Returns false when empty.
bool Dequeue(UIInputEvent& out);

// Flush everything (used on GameState transitions etc. as needed)
void Clear();

}  // namespace UIInput
