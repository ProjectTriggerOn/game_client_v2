#pragma once

//=============================================================================
// Weapon sight reticle.
//
// A single quad authored inside the sight's see-through window and drawn unlit
// so it stays bright regardless of scene lighting. Everything on the quad
// outside the dot is transparent and is discarded by the unlit pixel shader's
// clip(), which means scaling the quad in place scales only the visible dot --
// the size can be tuned without re-authoring the mesh or the texture.
//
// Owns the config key weapon.reticle_scale (docs 9.2: each module owns its
// keys), so both the first- and third-person draw paths share one knob.
//=============================================================================

#include <DirectXMath.h>

#include "model.h"

// Subscribes to weapon.reticle_scale. Safe to call repeatedly -- only the first
// call subscribes. Subscribe fires immediately when the key already has a
// value, so this doubles as the initial read.
void Reticle_Initialize();

// Centroid of the reticle mesh in its own model space; the scale in
// Reticle_Draw is applied about this point. Cache the result at load time.
//
// Assumes the model was loaded with ModelLoad(path, 1.0f) -- i.e. scale 1 and
// isBlender false -- so the raw aiMesh vertices match what reaches the GPU.
DirectX::XMFLOAT3 Reticle_GetCenter(MODEL* model);

// Draws the reticle scaled about `center` by weapon.reticle_scale. `world` is
// the transform that places the un-scaled quad on the sight.
void Reticle_Draw(MODEL* model, const DirectX::XMFLOAT3& center, const DirectX::XMMATRIX& world);
