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

// Scoring pushes. Stored when called (during Game_Update) and flushed at the top
// of UI::Render — the one safe JS-call point per frame. Kill-feed events use a
// queue (not a dirty flag) so multiple kills in one frame all surface.
void PushScores(int red, int blue);
void PushMatchTimer(float secondsRemaining);
void PushKillFeed(int killerId, int victimId, int killerTeam, int victimTeam);
void PushScoreboard(const char* json);
void PushScoreboardVisible(bool visible);
void PushMatchResult(const char* json);

// Page switch helper: calls Router.show('<name>'). UIPolicy_Apply drives this
// for state-derived pages; the ui_test sandbox uses it directly for preview.
void ShowPage(const char* name);

// Same as ShowPage but deferred to the start of the next UI::Render — safe to
// call from inside a JS callback (avoids re-entrant script evaluation).
void ShowPageDeferred(const char* name);

// Toggle the #curtain loading overlay (window.Curtain.show/hide; CSS fades the
// black opacity). Driven by the SceneTransition coordinator (scene.h) to mask
// the scene-swap init hitch. MUST be called from the main loop, never from
// inside a JS callback (it EvaluateScripts synchronously — see ShowPageDeferred).
void SetCurtain(bool visible);

// --- Hot reload helpers (Slice F, docs §10) ---------------------------------
// Called by UI::HotReload::Poll (main thread) when watched files change. These
// own the View, so the hot-reload module stays free of Ultralight headers.

// Full View::Reload. The JSContext is rebuilt, so OnDOMReady re-binds the bridge
// and router.js re-boots onto game.getBootPage() (the page for the current
// GameState) instead of always 'title'.
void ReloadView();

// Non-destructive stylesheet refresh: calls window.reloadCSS('<relPath>') which
// re-inserts the <link> with a cache-busting query. `relPath` is forward-slashed
// and relative to ui_src (e.g. "pages/game/hud.css").
void ReloadStyles(const char* relPath);

}  // namespace UI
