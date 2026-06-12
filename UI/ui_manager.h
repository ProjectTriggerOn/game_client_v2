#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace UI {

// How the UI layer absorbs input (docs §5.2). Slice C: one global value;
// Slice E will drive this from OnGameStateChanged.
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

void SetInteractiveLevel(InteractiveLevel level);
InteractiveLevel GetInteractiveLevel();
bool IsModalActive();   // == Modal. Game_Update uses this in Slice E to freeze gameplay input

// Slice B verification helper: calls EvaluateScript("Router.show('<name>')").
// Will be replaced by the proper JS Bridge in Slice D.
void ShowPage(const char* name);

// Same as ShowPage but deferred to the start of the next UI::Render — safe to
// call from inside a JS callback (avoids re-entrant script evaluation).
void ShowPageDeferred(const char* name);

}  // namespace UI
