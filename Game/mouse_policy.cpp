#include "mouse_policy.h"

#include "scene.h"
#include "game.h"
#include "mouse.h"
#include "ms_logger.h"
#include "ui_manager.h"

void MousePolicy_Apply()
{
    bool uiCursor = true;   // default for non-game scenes (ui_test / title / result)

    if (Scene_GetCurrent() == SCENE_GAME)
    {
        uiCursor = Game_WantsUICursor() || UI::IsModalActive();
    }

    Mouse_SetMode(uiCursor ? MOUSE_POSITION_MODE_ABSOLUTE
                           : MOUSE_POSITION_MODE_RELATIVE);
    Mouse_SetVisible(uiCursor);

    if (MSLogger_IsUIMode() != uiCursor)
        MSLogger_SetUIMode(uiCursor);
}
