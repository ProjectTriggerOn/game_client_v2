//=============================================================================
// map.cpp
//
// Map system implementation.
// Reads collider definitions from map_colliders.h, computes AABBs,
// renders visual objects, and registers colliders for physics.
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

using namespace DirectX;

namespace {
	// Runtime map objects (built from MAP_COLLIDERS at init)
	MapObject g_MapObjects[MAP_COLLIDER_COUNT];

	// Cube texture
	int g_CubeTexId = -1;
}

//-----------------------------------------------------------------------------
// Initialize — build runtime objects from shared collider data
//-----------------------------------------------------------------------------
void Map_Initialize()
{
	g_CubeTexId = Texture_LoadFromFile(L"resource/texture/stone_001.jpg");

	for (int i = 0; i < MAP_COLLIDER_COUNT; i++)
	{
		const MapColliderDef& def = MAP_COLLIDERS[i];
		MapObject& obj = g_MapObjects[i];

		obj.categoryId = def.category;
		obj.position = { def.posX, def.posY, def.posZ };
		obj.isGround = def.isGround;

		switch (def.category)
		{
		case MAP_CUBE:
			// Cube: compute AABB from position (±0.5 unit cube)
			obj.aabb = Cube_GetAABB(obj.position);
			break;

		case MAP_GROUND:
		case MAP_WALL:
		default:
			// Explicit AABB from definition
			obj.aabb = {
				{ def.minX, def.minY, def.minZ },
				{ def.maxX, def.maxY, def.maxZ }
			};
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Finalize
//-----------------------------------------------------------------------------
void Map_Finalize()
{
	// Texture cleanup handled by Texture system
}

//-----------------------------------------------------------------------------
// Draw — render all visible map objects
//-----------------------------------------------------------------------------
void Map_Draw()
{
	for (int i = 0; i < MAP_COLLIDER_COUNT; i++)
	{
		const MapObject& obj = g_MapObjects[i];

		switch (obj.categoryId)
		{
		case MAP_GROUND:
		{
			// Ground: rendered as MeshField
			XMMATRIX mtxW = XMMatrixTranslation(0.0f, -1.0f, 0.0f);
			MeshField_Draw(mtxW);
			break;
		}

		case MAP_CUBE:
		{
			// Cube: rendered with Cube_Draw at object position
			XMMATRIX mtxW = XMMatrixTranslation(
				obj.position.x, obj.position.y, obj.position.z);
			Cube_Draw(g_CubeTexId, mtxW);
			break;
		}

		case MAP_WALL:
			// Invisible wall — no rendering
			break;

		default:
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// RegisterColliders — push all AABBs into collision world
//-----------------------------------------------------------------------------
void Map_RegisterColliders(CollisionWorld& world)
{
	world.Clear();
	for (int i = 0; i < MAP_COLLIDER_COUNT; i++)
	{
		world.AddAABB(g_MapObjects[i].aabb, g_MapObjects[i].isGround);
	}
}

//-----------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------
int Map_GetObjectCount()
{
	return MAP_COLLIDER_COUNT;
}

const MapObject* Map_GetObject(int index)
{
	if (index < 0 || index >= MAP_COLLIDER_COUNT) return nullptr;
	return &g_MapObjects[index];
}
