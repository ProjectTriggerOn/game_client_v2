#pragma once
//=============================================================================
// map.h
//
// Map system — loads collider data from map_colliders.h,
// renders objects (MeshField, Cubes), and registers colliders.
//=============================================================================

#include <DirectXMath.h>
#include "collision.h"

// Forward declaration
class CollisionWorld;

// Map object instance (runtime data with computed AABB)
struct MapObject
{
	int   categoryId;
	DirectX::XMFLOAT3 position;
	AABB  aabb;
	bool  isGround;
};

void Map_Initialize();
void Map_Finalize();
void Map_Draw();

// Register all map colliders into a CollisionWorld
void Map_RegisterColliders(CollisionWorld& world);

// Accessors
int Map_GetObjectCount();
const MapObject* Map_GetObject(int index);
