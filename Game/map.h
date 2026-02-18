#pragma once
//=============================================================================
// map.h
//
// Map system — generates draw objects from grid data,
// renders visual objects, and registers colliders for physics.
//=============================================================================

#include <DirectXMath.h>

// Forward declaration
class CollisionWorld;

void Map_Initialize();
void Map_Finalize();
void Map_Draw();

// Register all map colliders into a CollisionWorld
void Map_RegisterColliders(CollisionWorld& world);
