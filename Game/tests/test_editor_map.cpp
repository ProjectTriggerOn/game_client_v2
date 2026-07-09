//=============================================================================
// test_editor_map.cpp — standalone round-trip test for EditorMap <-> .map.
// NOT part of TriggerOn.vcxproj (defines its own main). Build+run from repo root:
//   cmd /c "\"...\VsDevCmd.bat\" -arch=x64 -no_logo && cd /d D:\work_place\TriggerOn\game_client && cl /nologo /std:c++17 /EHsc /W4 /I Game Game\editor_map.cpp Game\tests\test_editor_map.cpp /Fe:_test_editor_map.exe && _test_editor_map.exe"
//=============================================================================
#define _CRT_SECURE_NO_WARNINGS
#include "editor_map.h"
#include "map_io.h"
#include <DirectXMath.h>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace editor;
using namespace DirectX;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); g_fail++; } } while (0)

static bool F3Eq(const XMFLOAT3& a, const XMFLOAT3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static bool MapsEqual(const EditorMap& a, const EditorMap& b) {
    if (a.meta.name != b.meta.name || a.meta.author != b.meta.author) return false;
    if (a.boxes.size() != b.boxes.size()) return false;
    for (size_t i = 0; i < a.boxes.size(); i++) {
        const auto& x = a.boxes[i]; const auto& y = b.boxes[i];
        if (!F3Eq(x.pos, y.pos) || !F3Eq(x.rotEuler, y.rotEuler) ||
            !F3Eq(x.scale, y.scale) || x.textureId != y.textureId) return false;
    }
    if (a.models.size() != b.models.size()) return false;
    for (size_t i = 0; i < a.models.size(); i++) {
        const auto& x = a.models[i]; const auto& y = b.models[i];
        if (x.asset != y.asset || !F3Eq(x.pos, y.pos) || !F3Eq(x.rotEuler, y.rotEuler) ||
            !F3Eq(x.scale, y.scale) || x.textureId != y.textureId) return false;
    }
    if (a.colliders.size() != b.colliders.size()) return false;
    for (size_t i = 0; i < a.colliders.size(); i++) {
        const auto& x = a.colliders[i]; const auto& y = b.colliders[i];
        if (!F3Eq(x.min, y.min) || !F3Eq(x.max, y.max) ||
            x.isGround != y.isGround || x.ownerModel != y.ownerModel) return false;
    }
    if (a.spawns.size() != b.spawns.size()) return false;
    for (size_t i = 0; i < a.spawns.size(); i++) {
        const auto& x = a.spawns[i]; const auto& y = b.spawns[i];
        if (!F3Eq(x.pos, y.pos) || x.yaw != y.yaw || x.team != y.team) return false;
    }
    if (a.lights.size() != b.lights.size()) return false;
    for (size_t i = 0; i < a.lights.size(); i++) {
        const auto& x = a.lights[i]; const auto& y = b.lights[i];
        if (x.type != y.type || !F3Eq(x.pos, y.pos) || !F3Eq(x.dir, y.dir) ||
            !F3Eq(x.color, y.color) || x.intensity != y.intensity) return false;
    }
    if (a.env.skyAsset != b.env.skyAsset) return false;
    if (!F3Eq(a.env.ambient, b.env.ambient) || !F3Eq(a.env.fogColor, b.env.fogColor)) return false;
    if (a.env.fogStart != b.env.fogStart || a.env.fogEnd != b.env.fogEnd) return false;
    return true;
}

int main() {
    // --- Test 1: synthetic all-fields round trip ---
    EditorMap a;
    a.meta.name = "unit-test-map";
    a.meta.author = "tester";
    a.boxes.push_back({ {1,2,3}, {0,0.5f,0}, {2,4,2}, 7 });
    a.boxes.push_back({ {-4,0.5f,5}, {0,0,0}, {1,1,1}, 0 });
    a.models.push_back({ "resource/model/crate.fbx", {3,0,3}, {0,1.57f,0}, {1,1,1}, 0 });
    a.colliders.push_back({ {-10,-1,-10}, {10,0,10}, true,  -1 });   // ground
    a.colliders.push_back({ {0,0,0},      {2,4,2},   false, -1 });
    a.colliders.push_back({ {-4,0,5},     {-3,1,6},  false, -1 });
    a.spawns.push_back({ {0,0,-7.5f}, 0.0f,     mapio::TEAM_RED });
    a.spawns.push_back({ {0,0, 7.5f}, 3.14159f, mapio::TEAM_BLUE });
    a.lights.push_back({ mapio::LIGHT_DIRECTIONAL, {0,0,0}, {0,-1,0}, {1,1,1},       1.0f });
    a.lights.push_back({ mapio::LIGHT_POINT,       {2,3,2}, {0,0,0},  {1,0.5f,0.2f}, 5.0f });
    a.env.skyAsset = "resource/model/sky.fbx";
    a.env.ambient  = { 0.2f, 0.2f, 0.2f };
    a.env.fogColor = { 0.1f, 0.1f, 0.15f };
    a.env.fogStart = 10.0f;
    a.env.fogEnd   = 200.0f;

    const char* tmp = "_editor_roundtrip.tmp.map";
    CHECK(EditorMap_Save(tmp, a), "save synthetic map");
    EditorMap b;
    CHECK(EditorMap_Load(tmp, b), "load synthetic map");
    CHECK(MapsEqual(a, b), "synthetic round-trip equal");
    std::remove(tmp);

    // --- Test 2: default.map loads, expected shape, checksum + byte-identical re-save ---
    EditorMap def;
    if (EditorMap_Load("resource/maps/default.map", def)) {
        CHECK(def.boxes.size()     == 144, "default.map has 144 boxes");
        CHECK(def.colliders.size() == 5,   "default.map has 5 colliders");
        CHECK(def.spawns.size()    == 2,   "default.map has 2 spawns");
        CHECK(def.meta.name == std::string("default"), "default.map name is 'default'");

        mapio::MapData d = EditorMap_ToMapData(def);
        CHECK(mapio::CollisionChecksum(d) == 0xe4f964e7u, "default.map collision checksum preserved");

        const char* re = "_editor_resave.tmp.map";
        CHECK(EditorMap_Save(re, def), "re-save default.map");
        FILE* f1 = std::fopen("resource/maps/default.map", "rb");
        FILE* f2 = std::fopen(re, "rb");
        bool ident = false;
        if (f1 && f2) {
            std::fseek(f1, 0, SEEK_END); long n1 = std::ftell(f1); std::fseek(f1, 0, SEEK_SET);
            std::fseek(f2, 0, SEEK_END); long n2 = std::ftell(f2); std::fseek(f2, 0, SEEK_SET);
            if (n1 == n2 && n1 > 0) {
                std::vector<unsigned char> b1((size_t)n1), b2((size_t)n2);
                size_t r1 = std::fread(b1.data(), 1, (size_t)n1, f1);
                size_t r2 = std::fread(b2.data(), 1, (size_t)n2, f2);
                ident = (r1 == (size_t)n1 && r2 == (size_t)n2 &&
                         std::memcmp(b1.data(), b2.data(), (size_t)n1) == 0);
            }
        }
        if (f1) std::fclose(f1);
        if (f2) std::fclose(f2);
        std::remove(re);
        CHECK(ident, "default.map re-save byte-identical");
    } else {
        std::printf("WARN: resource/maps/default.map not found — run from game_client/ to exercise it\n");
    }

    if (g_fail == 0) { std::printf("test_editor_map: ALL PASSED\n"); return 0; }
    std::printf("test_editor_map: %d FAILURE(S)\n", g_fail);
    return 1;
}
