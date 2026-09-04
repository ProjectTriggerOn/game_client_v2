// test_editor_input.cpp — standalone unit test for the pure half of editor_input.h.
// Build/run: see the plan's Step 2 (VsDevCmd + cl, wrapped in a .bat).
#include "../editor_input.h"

#include <cstdio>

static int g_fails = 0;

static void Check(bool cond, const char* what)
{
    if (!cond) { std::printf("FAIL: %s\n", what); ++g_fails; }
}

// A 1920x1080 client area with the default dock: 44px toolbar, 300px inspector.
static const editorinput::DockLayout kD{};
static const int W = 1920, H = 1080;

static void TestHitDock()
{
    // Toolbar strip: the full width of the top 44 rows.
    Check(editorinput::HitDock(kD, 0, 0, W, H),        "top-left corner is toolbar");
    Check(editorinput::HitDock(kD, 960, 43, W, H),     "y=43 is still toolbar");
    Check(editorinput::HitDock(kD, 1919, 10, W, H),    "toolbar spans full width");

    // Inspector column: right 300px, but only BELOW the toolbar.
    Check(editorinput::HitDock(kD, 1620, 44, W, H),    "inspector starts at x=W-300");
    Check(editorinput::HitDock(kD, 1919, 1079, W, H),  "bottom-right is inspector");

    // Viewport: everything else.
    Check(!editorinput::HitDock(kD, 960, 44, W, H),    "centre just below toolbar is viewport");
    Check(!editorinput::HitDock(kD, 1619, 500, W, H),  "one pixel left of inspector is viewport");
    Check(!editorinput::HitDock(kD, 0, 1079, W, H),    "bottom-left is viewport");

    // Outside the window is never the viewport (a drag that leaves the window
    // must not keep picking).
    Check(editorinput::HitDock(kD, -1, 500, W, H),     "left of window is not viewport");
    Check(editorinput::HitDock(kD, 500, -1, W, H),     "above window is not viewport");
    Check(editorinput::HitDock(kD, W, 500, W, H),      "right of window is not viewport");
    Check(editorinput::HitDock(kD, 500, H, W, H),      "below window is not viewport");
}

static void TestArbiterLatch()
{
    using editorinput::Owner;

    // A drag that STARTS in the viewport keeps the viewport even when the cursor
    // crosses a panel — otherwise a gizmo drag would break the moment it passed
    // under the inspector.
    editorinput::Arbiter a;
    Check(editorinput::Step(a, /*overUI*/false, /*down*/true) == Owner::Viewport, "press in viewport latches Viewport");
    Check(editorinput::Step(a, true,  true)                   == Owner::Viewport, "still Viewport while held over UI");
    Check(editorinput::Step(a, true,  false)                  == Owner::None,     "release unlatches");

    // A drag that STARTS on a panel never leaks a pick into the viewport.
    editorinput::Arbiter b;
    Check(editorinput::Step(b, true,  true)  == Owner::UI,   "press on panel latches UI");
    Check(editorinput::Step(b, false, true)  == Owner::UI,   "still UI while held over viewport");
    Check(editorinput::Step(b, false, false) == Owner::None, "release unlatches");

    // Hovering with no button held owns nothing.
    editorinput::Arbiter c;
    Check(editorinput::Step(c, false, false) == Owner::None, "hover over viewport owns nothing");
    Check(editorinput::Step(c, true,  false) == Owner::None, "hover over panel owns nothing");

    // Re-latching after a release picks up the NEW cursor location.
    editorinput::Arbiter d;
    Check(editorinput::Step(d, false, true)  == Owner::Viewport, "first press: viewport");
    Check(editorinput::Step(d, false, false) == Owner::None,     "release");
    Check(editorinput::Step(d, true,  true)  == Owner::UI,       "second press over panel: UI");
}

int main()
{
    TestHitDock();
    TestArbiterLatch();
    if (g_fails == 0) { std::printf("ALL PASSED\n"); return 0; }
    std::printf("%d CHECK(S) FAILED\n", g_fails);
    return 1;
}
