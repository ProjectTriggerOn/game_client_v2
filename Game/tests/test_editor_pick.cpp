//=============================================================================
// test_editor_pick.cpp — standalone ray-math test. NOT in the vcxproj.
// Build+run via a .bat wrapper (see the plan Global Constraints):
//   cl /nologo /std:c++17 /EHsc /W4 /I Game Game\editor_pick.cpp Game\tests\test_editor_pick.cpp /Fe:_test_editor_pick.exe && _test_editor_pick.exe
//=============================================================================
#include "editor_pick.h"
#include <DirectXMath.h>
#include <cstdio>
#include <cmath>

using namespace DirectX;
static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); g_fail++; } } while (0)
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

int main() {
    // Ray-AABB: straight down the +Z axis into a unit box at z in [4,6].
    Ray r; r.origin = { 0, 0, 0 }; r.direction = { 0, 0, 1 };
    AABB box{ { -1, -1, 4 }, { 1, 1, 6 } };
    float t = 0;
    CHECK(EditorPick_RayAABB(r, box, t), "ray hits box ahead");
    CHECK(Near(t, 4.0f), "entry t == 4");

    // Miss: parallel offset.
    Ray r2; r2.origin = { 5, 0, 0 }; r2.direction = { 0, 0, 1 };
    float t2 = 0;
    CHECK(!EditorPick_RayAABB(r2, box, t2), "parallel offset ray misses");

    // Behind: box behind the origin along -Z.
    AABB behind{ { -1, -1, -6 }, { 1, 1, -4 } };
    float t3 = 0;
    CHECK(!EditorPick_RayAABB(r, behind, t3), "box behind ray misses");

    // Ray-plane: ray from above pointing down hits y=0 ground.
    Ray rp; rp.origin = { 2, 5, 3 }; rp.direction = { 0, -1, 0 };
    XMFLOAT3 hit{};
    CHECK(EditorPick_RayPlane(rp, { 0, 0, 0 }, { 0, 1, 0 }, hit), "ray hits ground plane");
    CHECK(Near(hit.x, 2.0f) && Near(hit.y, 0.0f) && Near(hit.z, 3.0f), "ground hit at (2,0,3)");

    // Ray-plane parallel: horizontal ray never hits horizontal plane.
    Ray rp2; rp2.origin = { 0, 5, 0 }; rp2.direction = { 1, 0, 0 };
    XMFLOAT3 hit2{};
    CHECK(!EditorPick_RayPlane(rp2, { 0, 0, 0 }, { 0, 1, 0 }, hit2), "parallel ray misses plane");

    if (g_fail == 0) { std::printf("test_editor_pick: ALL PASSED\n"); return 0; }
    std::printf("test_editor_pick: %d FAILURE(S)\n", g_fail);
    return 1;
}
