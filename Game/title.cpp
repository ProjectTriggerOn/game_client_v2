#include "title.h"

// The title screen is now an Ultralight HTML page (ui_src/pages/title.*), shown
// by UIPolicy_Apply when SCENE_TITLE is active and rendered by UI::Render on top
// of the cleared frame. The menu buttons drive transitions through the C++ bridge
// (game.startLocalGame / game.quit) and the JS router, so this scene owns no
// native rendering or input — its lifecycle hooks are intentionally empty.

void Title_Initialize()
{
}

void Title_Finalize()
{
}

void Title_Update(double )
{
}

void Title_Draw()
{
}
