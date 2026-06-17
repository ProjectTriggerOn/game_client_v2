#include "ui_policy.h"

#include "scene.h"
#include "game.h"
#include "ui_manager.h"

void UIPolicy_Apply()
{
    static scene     s_lastScene = SCENE_MAX;
    static GameState s_lastState = (GameState)0xFF;

    const scene s = Scene_GetCurrent();

    if (s == SCENE_GAME)
    {
        const GameState gs = Game_GetState();

        UI::InteractiveLevel level;
        const char* page;
        switch (gs)
        {
        case PAUSE:   level = UI::InteractiveLevel::Modal;   page = "pause";    break;
        case SETTING: level = UI::InteractiveLevel::Modal;   page = "settings"; break;
        default:      level = UI::InteractiveLevel::Display; page = "hud";      break; // PLAY etc.
        }

        UI::SetInteractiveLevel(level);                 // idempotent, reassert each frame
        if (s != s_lastScene || gs != s_lastState)      // switch page only on transition
            UI::ShowPageDeferred(page);

        s_lastState = gs;
    }
    else
    {
        // Sandbox / non-game scenes: always interactive. Page is left to the
        // sandbox (UITest F-keys) so it can preview any page freely — only force
        // "title" on the initial entry into the scene.
        UI::SetInteractiveLevel(UI::InteractiveLevel::Interactive);
        if (s != s_lastScene)
            UI::ShowPageDeferred("title");
    }

    s_lastScene = s;
}
