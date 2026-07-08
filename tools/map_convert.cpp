// Converts the compile-time MAP_GRID / MAP_COLLIDERS into resource/maps/default.map.
// Build & run (from game_client/):
//   g++ -std=c++17 -Wall -Wextra -I Game tools/map_convert.cpp -o map_convert
//   ./map_convert            # writes resource/maps/default.map
#include "map_io.h"
#include "map_colliders.h"
#include <cstdio>
#include <cstring>

using namespace mapio;

int main() {
    MapData d{};
    std::strncpy(d.name,   "default",   sizeof(d.name) - 1);
    std::strncpy(d.author, "converter", sizeof(d.author) - 1);

    // Collision section: the current merged AABBs, verbatim.
    for (int i = 0; i < MAP_COLLIDER_COUNT; i++) {
        const MapColliderDef& c = MAP_COLLIDERS[i];
        d.colliders.push_back(MapAABB{ c.minX, c.minY, c.minZ, c.maxX, c.maxY, c.maxZ,
                                       static_cast<uint8_t>(c.isGround ? 1 : 0), {0,0,0} });
    }

    // Visual section: one unit box-brush per grid cube, matching Map_Initialize exactly.
    for (int row = 0; row < MAP_GRID_ROWS; row++) {
        for (int col = 0; col < MAP_GRID_COLS; col++) {
            if (MAP_GRID[row][col] == 0) continue;
            float worldX = (float)col + MAP_OFFSET_X + 0.5f;
            float worldZ = (float)row + MAP_OFFSET_Z + 0.5f;
            for (int h = 0; h < MAP_BLOCK_HEIGHT; h++) {
                float worldY = (float)h + 0.5f;
                MapModelRef m{};
                std::strncpy(m.asset, "__box__", sizeof(m.asset) - 1);
                m.pos[0] = worldX; m.pos[1] = worldY; m.pos[2] = worldZ;
                m.scale[0] = m.scale[1] = m.scale[2] = 1.0f;
                m.textureId = 0;   // 0 => the default stone texture
                d.models.push_back(m);
            }
        }
    }

    // Spawns: current server strip midpoints (carried; consumed in a later plan).
    d.spawns.push_back(MapSpawn{ 0.0f, 0.0f, -7.5f, 0.0f,       TEAM_RED,  {0,0,0} });
    d.spawns.push_back(MapSpawn{ 0.0f, 0.0f,  7.5f, 3.14159f,   TEAM_BLUE, {0,0,0} });

    // Environment.
    std::strncpy(d.env.skyAsset, "resource/model/sky.fbx", sizeof(d.env.skyAsset) - 1);
    d.env.ambient[0] = d.env.ambient[1] = d.env.ambient[2] = 0.2f;

    if (!Write("resource/maps/default.map", d)) {
        std::fprintf(stderr, "ERROR: could not write resource/maps/default.map\n");
        return 1;
    }
    std::printf("Wrote resource/maps/default.map: %zu colliders, %zu boxes, checksum=%08x\n",
                d.colliders.size(), d.models.size(), CollisionChecksum(d));
    return 0;
}
