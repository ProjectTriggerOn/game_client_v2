//=============================================================================
// test_shipment_map.cpp — invariants over the generated resource/maps/shipment.map.
//
// NOT part of TriggerOn.vcxproj (it defines its own main). Build and run from
// the repo root, after tools/shipment_convert.cpp has produced the map:
//
//   cl /nologo /std:c++17 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I Game
//      Game\tests\test_shipment_map.cpp /Fe:_test_shipment_map.exe
//   _test_shipment_map.exe
//
// The generator already checks these before it writes anything, so this is a
// regression guard on the file that actually ships: it catches a stale map, a
// hand-edited one, or a generator change that silently loosened a rule. It
// deliberately asserts properties rather than byte-identical output, so
// re-dressing the yard does not break the test.
//=============================================================================
#include "map_io.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace mapio;

namespace {

int g_fail = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { std::printf("FAIL: %s\n", (msg)); g_fail++; } } while (0)

// Yard metrics, mirrored from tools/shipment_convert.cpp, where the edge is
// derived from the perimeter containers so they sit flush against it.
constexpr float HX = 15.80f;
constexpr float HZ = 15.06f;
constexpr float PLAYER_RADIUS = 0.35f;
constexpr float PLAYER_HEIGHT = 1.70f;

// The boundary clips are the only colliders taller than a stacked container,
// which is what tells them apart from geometry when reading the file back.
bool IsBoundaryClip(const MapAABB& c) { return c.maxY > 5.0f; }

bool Overlaps(const MapAABB& a, const MapAABB& b, float eps = 1e-3f) {
    return a.maxX - b.minX > eps && b.maxX - a.minX > eps
        && a.maxY - b.minY > eps && b.maxY - a.minY > eps
        && a.maxZ - b.minZ > eps && b.maxZ - a.minZ > eps;
}

bool IsBoxBrush(const MapModelRef& m) {
    return std::strcmp(m.asset, "__box__") == 0;
}

} // namespace

int main() {
    MapData d;
    if (!mapio::Read("resource/maps/shipment.map", d)) {
        std::printf("FAIL: cannot read resource/maps/shipment.map "
                    "(build and run tools/shipment_convert.cpp first)\n");
        return 1;
    }
    std::printf("shipment.map: name='%s' author='%s' colliders=%zu models=%zu "
                "spawns=%zu lights=%zu checksum=%08x\n",
                d.name, d.author, d.colliders.size(), d.models.size(),
                d.spawns.size(), d.lights.size(), CollisionChecksum(d));

    CHECK(std::strcmp(d.name, "shipment") == 0, "map is named 'shipment'");

    // --- Shape -------------------------------------------------------------
    CHECK(d.colliders.size() >= 25, "at least 25 colliders");
    // Guards against a map that generated but lost its dressing, not against
    // a legitimately tidier layout — replacing prop heaps with single crates
    // dropped this from 192 to 142 without losing anything.
    CHECK(d.models.size()    >= 120, "the yard is still dressed");
    CHECK(d.spawns.size()    == 12, "12 spawns (6 per team)");
    CHECK(d.lights.size()    == 5,  "one sun plus four lamp fills");

    // --- Ground and enclosure ----------------------------------------------
    bool ground = false;
    int clips = 0;
    for (const auto& c : d.colliders) {
        if (c.isGround && c.minY <= -1.0f && c.maxY >= 0.0f) ground = true;
        if (IsBoundaryClip(c)) clips++;
    }
    CHECK(ground, "ground collider present (Y -1..0, isGround)");
    CHECK(clips == 4, "four boundary clips enclose the yard");

    // --- Geometry is FBX, not box brushes ----------------------------------
    // The yard is built from real assets; only the ground slab is still a box.
    // A regression here means someone reintroduced textured cubes as geometry.
    int boxBrushes = 0, containers = 0, badAssetPaths = 0;
    for (const auto& m : d.models) {
        if (IsBoxBrush(m)) { boxBrushes++; continue; }
        const std::string asset(m.asset);
        if (asset.rfind("resource/model/", 0) != 0 ||
            asset.size() < 5 || asset.compare(asset.size() - 4, 4, ".fbx") != 0)
            badAssetPaths++;
        if (asset.find("SM_Container") != std::string::npos) containers++;
    }
    CHECK(boxBrushes <= 2, "at most two box brushes (the ground slabs)");
    CHECK(badAssetPaths == 0, "every prop is a resource/model/*.fbx path");
    CHECK(containers >= 16, "at least the 16 containers the layout calls for");

    // --- No collider may overlap another -----------------------------------
    // This is the failure that made the previous map unusable: containers
    // written twice, crossing through one another.
    int overlaps = 0;
    for (size_t i = 0; i < d.colliders.size(); i++) {
        for (size_t j = i + 1; j < d.colliders.size(); j++) {
            if (d.colliders[i].isGround || d.colliders[j].isGround) continue;
            if (Overlaps(d.colliders[i], d.colliders[j])) {
                if (overlaps < 5)
                    std::printf("      overlap: collider %zu x %zu\n", i, j);
                overlaps++;
            }
        }
    }
    CHECK(overlaps == 0, "no two colliders share volume");

    // --- Solid geometry stays inside the yard ------------------------------
    int outOfBounds = 0;
    for (const auto& c : d.colliders) {
        if (c.isGround || IsBoundaryClip(c)) continue;
        if (c.minX < -HX - 1e-3f || c.maxX > HX + 1e-3f ||
            c.minZ < -HZ - 1e-3f || c.maxZ > HZ + 1e-3f)
            outOfBounds++;
    }
    CHECK(outOfBounds == 0, "every collider is inside the yard");

    // --- The perimeter is sealed -------------------------------------------
    // A container parked against the yard edge must be flush with it. An 0.8 m
    // gap behind one is wide enough for the player to slip in and run the whole
    // perimeter, which is not a route this layout has.
    int perimeterGaps = 0;
    for (const auto& c : d.colliders) {
        if (c.isGround || IsBoundaryClip(c)) continue;
        const float gaps[4] = { c.minX + HX, HX - c.maxX, c.minZ + HZ, HZ - c.maxZ };
        for (float g : gaps)
            if (g > 0.01f && g < 1.5f) perimeterGaps++;
    }
    CHECK(perimeterGaps == 0, "perimeter geometry sits flush against the yard edge");

    // --- Spawns ------------------------------------------------------------
    int red = 0, blue = 0, blocked = 0, misfacing = 0;
    for (const auto& s : d.spawns) {
        (s.team == TEAM_RED ? red : blue)++;
        CHECK(s.team == TEAM_RED ? s.x < 0.0f : s.x > 0.0f,
              "RED spawns west of centre, BLUE east");

        const MapAABB capsule{ s.x - PLAYER_RADIUS, s.y + 0.05f, s.z - PLAYER_RADIUS,
                               s.x + PLAYER_RADIUS, s.y + PLAYER_HEIGHT, s.z + PLAYER_RADIUS,
                               0, {0, 0, 0} };
        for (const auto& c : d.colliders) {
            if (c.isGround) continue;
            if (Overlaps(capsule, c)) { blocked++; break; }
        }

        // The engine's forward vector is (sin yaw, 0, cos yaw); every spawn
        // should look at the middle rather than into the nearest wall.
        const float len = std::sqrt(s.x * s.x + s.z * s.z);
        if (len > 0.001f) {
            const float dot = (std::sin(s.yaw) * (-s.x) + std::cos(s.yaw) * (-s.z)) / len;
            if (dot < 0.95f) {
                misfacing++;
                std::printf("      spawn (%.1f, %.1f) faces away: dot=%.3f\n", s.x, s.z, dot);
            }
        }
    }
    CHECK(red == 6 && blue == 6, "six spawns per team");
    CHECK(blocked == 0, "no spawn sits inside geometry");
    CHECK(misfacing == 0, "every spawn faces the middle of the yard");

    // Mirror symmetry across x: neither team gets a better set of positions.
    int unmatched = 0;
    for (const auto& a : d.spawns) {
        bool found = false;
        for (const auto& b : d.spawns) {
            if (b.team == a.team) continue;
            if (std::fabs(b.x + a.x) < 1e-3f && std::fabs(b.z - a.z) < 1e-3f) {
                found = true;
                break;
            }
        }
        if (!found) unmatched++;
    }
    CHECK(unmatched == 0, "the two teams' spawns mirror across x");

    // --- Environment -------------------------------------------------------
    CHECK(d.env.skyAsset[0] != '\0', "sky asset authored");
    // The yard's diagonal is about 45 m, so fog starting inside it would haze
    // the play space instead of the scenery beyond the wall.
    CHECK(d.env.fogStart >= 32.0f, "fog starts past the far wall");
    CHECK(d.env.fogEnd > d.env.fogStart, "fog end is beyond fog start");
    for (int i = 0; i < 3; i++)
        CHECK(d.env.ambient[i] > 0.05f && d.env.ambient[i] < 0.95f,
              "ambient stays in the readable range");

    int suns = 0, points = 0;
    for (const auto& l : d.lights) (l.type == LIGHT_DIRECTIONAL ? suns : points)++;
    CHECK(suns == 1, "exactly one directional sun");
    CHECK(points == 4, "four point lights, one per corner lamp");

    if (g_fail == 0) std::printf("test_shipment_map: all checks passed\n");
    else             std::printf("test_shipment_map: %d check(s) FAILED\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
