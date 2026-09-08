#pragma once

#include <DirectXMath.h>
#include <unordered_map>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#include <d3d11.h>

#include "collision.h"
#pragma comment (lib, "assimp-vc143-mt.lib")



struct MODEL
{
	const aiScene* AiScene = nullptr;

	ID3D11Buffer** VertexBuffer;
	ID3D11Buffer** IndexBuffer;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture;

	// Bounds of the vertices ModelLoad actually uploaded, with the node
	// transform, the load-time scale and the axis swap all baked in.
	// aiMesh::mAABB describes the raw mesh and no longer matches what gets
	// drawn, so ModelGetAABB reads these instead.
	DirectX::XMFLOAT3 LocalMin{};
	DirectX::XMFLOAT3 LocalMax{};
};


// bakeNodeTransforms folds each mesh's accumulated node transform into the
// vertices. It defaults OFF because the first-person rig is authored to be
// drawn WITHOUT it: the arms, their reticles and the third-person characters
// all carry a x100 axis-conversion node that ModelDraw has always ignored, so
// baking it moves the reticle out of the rig's space and off screen. Map and
// editor content goes through ModelCatalog_Get, which turns it on — there the
// transform carries the asset's real units and orientation.
MODEL* ModelLoad(const char* FileName, float scale = 1.0f, bool isBlender = false,
                 bool bakeNodeTransforms = false);
void ModelRelease(MODEL* model);
void ModelDraw(MODEL* model,const DirectX::XMMATRIX& mtxW);
void ModelDrawUnlit(MODEL* model, const DirectX::XMMATRIX& mtxW);
AABB ModelGetAABB(MODEL* model,const DirectX::XMFLOAT3& position);
