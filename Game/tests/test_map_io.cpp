// Standalone round-trip test for map_io.h. No game engine, no DirectXMath.
// Build: g++ -std=c++17 -Wall -Wextra -I ../ test_map_io.cpp -o test_map_io && ./test_map_io
#include "../map_io.h"
#include <cstdio>
#include <cstring>
#include <cassert>

using namespace mapio;

static MapData MakeSample() {
    MapData d{};
    std::strncpy(d.name,   "unit_test_map", sizeof(d.name) - 1);
    std::strncpy(d.author, "tester",        sizeof(d.author) - 1);
    d.colliders.push_back(MapAABB{ -128,-1,-128, 128,0,128, 1, {0,0,0} });   // ground
    d.colliders.push_back(MapAABB{ -5,0,-5, -2,4,-2, 1, {0,0,0} });          // a block
    d.spawns.push_back(MapSpawn{ 0,0,-8, 0.0f, TEAM_RED,  {0,0,0} });
    d.spawns.push_back(MapSpawn{ 0,0, 8, 3.14f, TEAM_BLUE, {0,0,0} });
    MapModelRef m{}; std::strncpy(m.asset, "__box__", sizeof(m.asset)-1);
    m.pos[0]=1; m.pos[1]=2; m.pos[2]=3; m.scale[0]=m.scale[1]=m.scale[2]=1; m.textureId=7;
    d.models.push_back(m);
    d.lights.push_back(MapLight{ 0, {0,0,0}, {10,20,30}, {-1,-1,-1}, {1,1,1}, 1.0f });
    std::strncpy(d.env.skyAsset, "resource/model/sky.fbx", sizeof(d.env.skyAsset)-1);
    d.env.ambient[0]=0.2f; d.env.ambient[1]=0.2f; d.env.ambient[2]=0.2f;
    return d;
}

static bool AabbEq(const MapAABB& a, const MapAABB& b) {
    return std::memcmp(&a, &b, sizeof(MapAABB)) == 0;
}

int main() {
    const char* path = "test_roundtrip.map";
    MapData in = MakeSample();
    uint32_t sumIn = CollisionChecksum(in);

    assert(Write(path, in));

    MapData out{};
    assert(Read(path, out));

    // Metadata
    assert(std::strcmp(in.name, out.name) == 0);
    assert(std::strcmp(in.author, out.author) == 0);
    // Collision section round-trips exactly
    assert(in.colliders.size() == out.colliders.size());
    for (size_t i = 0; i < in.colliders.size(); i++) assert(AabbEq(in.colliders[i], out.colliders[i]));
    assert(in.spawns.size() == out.spawns.size());
    assert(std::memcmp(in.spawns.data(), out.spawns.data(), in.spawns.size()*sizeof(MapSpawn)) == 0);
    // Visual section round-trips exactly
    assert(in.models.size() == out.models.size());
    assert(std::memcmp(in.models.data(), out.models.data(), in.models.size()*sizeof(MapModelRef)) == 0);
    assert(in.lights.size() == out.lights.size());
    assert(std::memcmp(&in.env, &out.env, sizeof(MapEnv)) == 0);
    // Checksum is stable and covers only collision data
    uint32_t sumOut = CollisionChecksum(out);
    assert(sumIn == sumOut);

    std::remove(path);
    std::printf("test_map_io: ALL PASSED (checksum=%08x)\n", sumIn);
    return 0;
}
