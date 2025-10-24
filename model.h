#pragma once

#include <DirectXMath.h>
#include <unordered_map>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")



struct MODEL
{
	const aiScene* AiScene = nullptr;

	ID3D11Buffer** VertexBuffer;
	ID3D11Buffer** IndexBuffer;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture;
};


MODEL* ModelLoad(const char* FileNam,float scale = 1.0f);
void ModelRelease(MODEL* model);
void ModelDraw(MODEL* model,const DirectX::XMMATRIX& mtxW);
