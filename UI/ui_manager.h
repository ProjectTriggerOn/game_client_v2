#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace UI {

// How the UI layer absorbs input (docs §5.2). Derived each frame from
// (scene, GameState) by UIPolicy_Apply (ui_policy.h) — do not set imperatively
// outside that single owner.
enum class InteractiveLevel {
    Display,      // draw only, no input (HUD during PLAY)
    Interactive,  // consumes input normally (full-screen pages like TITLE)
    Modal,        // consumes input AND requires gameplay input freeze (PAUSE/SETTINGS)
};

void Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int backBufW, int backBufH);
void Finalize();
void Render();

// Call once per frame (after KeyLogger/MSLogger updates): drains UI_InputQueue,
// then drops or translates events into Ultralight and fires them at the View
// according to the current InteractiveLevel.
void ProcessInput();

void SetInteractiveLevel(InteractiveLevel level);  // owner: UIPolicy_Apply only
InteractiveLevel GetInteractiveLevel();
bool IsModalActive();   // == Modal. Read by MousePolicy_Apply to free the cursor.

// C++ → JS HUD data push (facade over UI::Bridge; docs §8.2). No-op if the HUD
// page hasn't defined the matching window.on*Changed handler.
void PushHealth(int current, int maxHp);
void PushAmmo(int current, int reserve);

// Page switch helper: calls Router.show('<name>'). UIPolicy_Apply drives this
// for state-derived pages; the ui_test sandbox uses it directly for preview.
void ShowPage(const char* name);

// Same as ShowPage but deferred to the start of the next UI::Render — safe to
// call from inside a JS callback (avoids re-entrant script evaluation).
void ShowPageDeferred(const char* name);

}  // namespace UI
