#include "scene_ui_test.h"

#include "key_logger.h"
#include "ui_manager.h"

void UITest_Initialize()
{
    // UI consumes all input. Cursor mode follows automatically: MousePolicy_Apply
    // gives every non-game scene a free, visible cursor.
    UI::SetInteractiveLevel(UI::InteractiveLevel::Interactive);

    // Note: the View's DOM is most likely not ready yet (LoadURL is async),
    // so do NOT call UI::ShowPage here — router.js shows the title page itself
    // once it finishes initializing. The proper "C++ waits for DOM ready, then
    // pushes state" flow arrives with Slice D's OnDOMReady callback.
}

void UITest_Finalize()
{
}

void UITest_Update(double elapsed_time)
{
    (void)elapsed_time;

    // Page-switch shortcuts (replaced by game.setState once Slice E lands)
    if (KeyLogger_IsTrigger(KK_F1)) UI::ShowPage("hud");
    if (KeyLogger_IsTrigger(KK_F2)) UI::ShowPage("title");
}

void UITest_Draw()
{
    // Intentionally empty: UI::Render is invoked centrally by the main.cpp
    // frame loop, and this scene has no 3D/2D content of its own — the
    // backdrop is just Direct3D_Clear's color.
}
