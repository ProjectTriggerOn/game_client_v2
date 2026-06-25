#include "ui_policy.h"

#include "scene.h"
#include "game.h"
#include "ui_manager.h"

namespace {

// Scenes whose page is driven by GameState (vs free JS navigation).
bool IsStateDriven(scene s)
{
    return s == SCENE_GAME;
}

// State-driven derivation: page + level from GameState.
void DeriveGamePage(GameState gs, UI::InteractiveLevel& level, const char*& page)
{
    switch (gs)
    {
    case PAUSE:   level = UI::InteractiveLevel::Modal;   page = "pause";    break;
    case SETTING: level = UI::InteractiveLevel::Modal;   page = "settings"; break;
    default:      level = UI::InteractiveLevel::Display; page = "hud";      break; // PLAY etc.
    }
}

// Navigation-driven scene: fixed level + the scene's entry page (JS owns the
// sub-navigation afterwards). New nav scenes (lobby) add a case here.
void NavScenePolicy(scene s, UI::InteractiveLevel& level, const char*& entryPage)
{
    level = UI::InteractiveLevel::Interactive;
    switch (s)
    {
    case SCENE_TITLE: entryPage = "title"; break;   // real Ultralight title menu
    // case SCENE_LOBBY: entryPage = "lobby-home"; break;
    default:          entryPage = "title"; break;   // ui_test sandbox / result
    }
}

}  // namespace

const char* UIPolicy_DerivePage()
{
    const scene s = Scene_GetCurrent();
    UI::InteractiveLevel level;
    const char* page = "title";
    if (IsStateDriven(s)) DeriveGamePage(Game_GetState(), level, page);
    else                  NavScenePolicy(s, level, page);
    return page;
}

void UIPolicy_Apply()
{
    static scene     s_lastScene = SCENE_MAX;
    static GameState s_lastState = (GameState)0xFF;

    const scene s = Scene_GetCurrent();
    UI::InteractiveLevel level;
    const char* page;

    if (IsStateDriven(s))
    {
        // Page + level are a pure function of GameState; reassert level every
        // frame (idempotent), force the page only on a state/scene transition.
        const GameState gs = Game_GetState();
        DeriveGamePage(gs, level, page);

        UI::SetInteractiveLevel(level);
        if (s != s_lastScene || gs != s_lastState)
            UI::ShowPageDeferred(page);

        s_lastState = gs;
    }
    else
    {
        // Navigation-driven: set the entry page + level once on scene-enter, then
        // leave page control to JS (Router.navigate/back, ui_test F-keys).
        NavScenePolicy(s, level, page);

        UI::SetInteractiveLevel(level);
        if (s != s_lastScene)
            UI::ShowPageDeferred(page);
    }

    s_lastScene = s;
}
