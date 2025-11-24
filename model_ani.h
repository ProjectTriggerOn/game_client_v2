#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <DirectXMath.h>
#include <unordered_map>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")
#include "assimp/scene.h"

struct Vertex3D_Ani
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT2 uv;
	DirectX::XMFLOAT4 weights;
	DirectX::XMUINT4 bones;
};

struct Bone
{
	std::string name;
	DirectX::XMFLOAT4X4 offsetMatrix; // Inverse Bind Pose Matrix
	DirectX::XMFLOAT4X4 localMatrix;  // Local transformation (relative to parent)
	DirectX::XMFLOAT4X4 globalMatrix; // Global transformation (relative to model root)
	
	int parentIndex = -1;
	std::vector<int> children;
};

struct KeyFrame
{
	double time;
	DirectX::XMFLOAT3 value; // Position or Scale
};

struct KeyFrameRot
{
	double time;
	DirectX::XMFLOAT4 value; // Quaternion
};

struct AnimationChannel
{
	std::string boneName;
	int boneIndex;
	std::vector<KeyFrame> positionKeys;
	std::vector<KeyFrameRot> rotationKeys;
	std::vector<KeyFrame> scalingKeys;
};

struct Animation
{
	std::string name;
	double duration;
	double ticksPerSecond;
	std::vector<AnimationChannel> channels;
};

struct MODEL_ANI
{
	const aiScene* AiScene = nullptr;
	
	struct Mesh
	{
		ID3D11Buffer* VertexBuffer;
		ID3D11Buffer* IndexBuffer;
		unsigned int IndexCount;
		int MaterialIndex;
	};
	std::vector<Mesh> Meshes;
	
	std::unordered_map<std::string, ID3D11ShaderResourceView*> Textures;
	
	std::vector<Bone> Bones;
	std::map<std::string, int> BoneMapping; // Name to Index
	
	std::vector<Animation> Animations;
	int CurrentAnimationIndex = 0;
	double CurrentTime = 0.0;
	
	DirectX::XMFLOAT4X4 GlobalInverseTransform;
};

MODEL_ANI* ModelAni_Load(const char* FileName, float scale = 1.0f);
void ModelAni_Release(MODEL_ANI* model);
void ModelAni_Update(MODEL_ANI* model, double elapsedTime);
void ModelAni_Draw(MODEL_ANI* model, const DirectX::XMMATRIX& mtxW);
void ModelAni_SetAnimation(MODEL_ANI* model, int index);
