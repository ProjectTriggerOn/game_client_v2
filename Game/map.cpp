//=============================================================================
// map.cpp
//
// Map system implementation.
// Generates cube draw positions from MAP_GRID[][],
// registers merged AABBs from MAP_COLLIDERS[].
//=============================================================================

#include "map.h"
#include "map_colliders.h"
#include "collision_world.h"
#include "cube.h"
#include "mesh_field.h"
#include "texture.h"
#include "shader_3d.h"
#include "light.h"
#include "player_cam_fps.h"
#include <DirectXMath.h>
#include <vector>

using namespace DirectX;

namespace {
	// Cube draw positions generated from grid
	struct CubeInstance
	{
		XMFLOAT3 position;
	};

	std::vector<CubeInstance> g_CubeInstances;

	// Cube texture
	int g_CubeTexId = -1;
}

//-----------------------------------------------------------------------------
// Initialize — generate cube instances from grid data
//-----------------------------------------------------------------------------
void Map_Initialize()
{
	g_CubeTexId = Texture_LoadFromFile(L"resource/texture/stone_001.jpg");

	// Generate cube instances from grid
	g_CubeInstances.clear();
	for (int row = 0; row < MAP_GRID_ROWS; row++)
	{
		for (int col = 0; col < MAP_GRID_COLS; col++)
		{
			if (MAP_GRID[row][col] == 0) continue;

			float worldX = (float)col + MAP_OFFSET_X + 0.5f;
			float worldZ = (float)row + MAP_OFFSET_Z + 0.5f;

			// Stack cubes vertically
			for (int h = 0; h < MAP_BLOCK_HEIGHT; h++)
			{
				float worldY = (float)h + 0.5f;
				g_CubeInstances.push_back({ { worldX, worldY, worldZ } });
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Finalize
//-----------------------------------------------------------------------------
void Map_Finalize()
{
	g_CubeInstances.clear();
}

//-----------------------------------------------------------------------------
// Draw — render ground + all cube instances
//-----------------------------------------------------------------------------
void Map_Draw()
{
	// Ground
	XMMATRIX mtxW = XMMatrixTranslation(0.0f, -1.0f, 0.0f);
	MeshField_Draw(mtxW);

	// Cubes
	for (const auto& inst : g_CubeInstances)
	{
		mtxW = XMMatrixTranslation(inst.position.x, inst.position.y, inst.position.z);
		Cube_Draw(g_CubeTexId, mtxW);
	}
}

//-----------------------------------------------------------------------------
// RegisterColliders — push merged AABBs into collision world
//-----------------------------------------------------------------------------
void Map_RegisterColliders(CollisionWorld& world)
{
	world.Clear();
	for (int i = 0; i < MAP_COLLIDER_COUNT; i++)
	{
		const MapColliderDef& def = MAP_COLLIDERS[i];
		AABB aabb = {
			{ def.minX, def.minY, def.minZ },
			{ def.maxX, def.maxY, def.maxZ }
		};
		world.AddAABB(aabb, def.isGround);
	}
}
