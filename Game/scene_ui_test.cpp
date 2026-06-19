#include "scene_ui_test.h"

#include "key_logger.h"
#include "ui_manager.h"

void UITest_Initialize()
{
    // Input level + cursor are derived automatically: UIPolicy_Apply forces
    // Interactive for non-game scenes, MousePolicy_Apply gives a free visible
    // cursor. The DOM may not be ready yet (LoadURL is async) — router.js shows
    // the title page itself once it finishes loading.
}

void UITest_Finalize()
{
}

void UITest_Update(double elapsed_time)
{
    (void)elapsed_time;

    // Sandbox page preview. UIPolicy_Apply only forces a page on scene entry for
    // non-game scenes, so these manual switches stick — handy for eyeballing any
    // page (incl. in-game ones) without launching gameplay.
    // Use F5-F8 (not F1-F4): F1 is the in-game debug-collision toggle, keep that
    // key's meaning consistent across scenes.
    if (KeyLogger_IsTrigger(KK_F5)) UI::ShowPage("title");
    if (KeyLogger_IsTrigger(KK_F6)) UI::ShowPage("hud");
    if (KeyLogger_IsTrigger(KK_F7)) UI::ShowPage("pause");
    if (KeyLogger_IsTrigger(KK_F8)) UI::ShowPage("settings");
}

void UITest_Draw()
{
    // Intentionally empty: UI::Render is invoked centrally by the main.cpp
    // frame loop, and this scene has no 3D/2D content of its own — the
    // backdrop is just Direct3D_Clear's color.
}
