#include "model_ani.h"
#include "direct3d.h"
#include "texture.h"
#include "shader_3d_ani.h"
#include "WICTextureLoader11.h"
#include <iostream>
#include <algorithm>
#include <set>

using namespace DirectX;

namespace
{
	int g_TextureWhiteAni = -1;

	XMFLOAT4X4 ToXMFLOAT4X4(const aiMatrix4x4& aiMat)
	{
		// Transpose for DirectX (Row-Major)
		return XMFLOAT4X4(
			aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
			aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
			aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
			aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
		);
	}

	XMMATRIX ToXMMATRIX(const aiMatrix4x4& aiMat)
	{
		// Transpose for DirectX (Row-Major)
		return XMMATRIX(
			aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
			aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
			aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
			aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
		);
	}
	
	// Helper to find keyframe index
	template <typename T>
	int FindKeyIndex(double animationTime, const std::vector<T>& keys)
	{
		for (size_t i = 0; i < keys.size() - 1; i++)
		{
			if (animationTime < keys[i + 1].time)
				return (int)i;
		}
		return 0;
	}
}

MODEL_ANI* ModelAni_Load(const char* FileName, float scale)
{
	MODEL_ANI* model = new MODEL_ANI;

	unsigned int flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights  |  aiProcess_ConvertToLeftHanded;
	model->AiScene = aiImportFile(FileName, flags);

	if (!model->AiScene)
	{
		delete model;
		return nullptr;
	}

	XMFLOAT4X4 rootTransform = ToXMFLOAT4X4(model->AiScene->mRootNode->mTransformation);
	XMMATRIX globalInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&rootTransform));

	// Global Inverse Transform
	XMStoreFloat4x4(&model->GlobalInverseTransform, globalInv);

	// Load Textures (Similar to ModelLoad)
	// ... (Simplified for brevity, assuming external textures or embedded)
	// Copying texture loading logic from model.cpp
	std::string modelPath(FileName);
	size_t pos = modelPath.find_last_of("/\\");
	std::string directory = (pos != std::string::npos) ? modelPath.substr(0, pos) : "";
	
	// Load Embedded Textures
	for (unsigned int i = 0; i < model->AiScene->mNumTextures; i++)
	{
		aiTexture* aitexture = model->AiScene->mTextures[i];
		ID3D11ShaderResourceView* texture = nullptr;
		ID3D11Resource* resource = nullptr;
		CreateWICTextureFromMemory(Direct3D_GetDevice(), Direct3D_GetDeviceContext(), (const uint8_t*)aitexture->pcData, aitexture->mWidth, &resource, &texture);
		if(texture) {
			resource->Release();
			model->Textures[aitexture->mFilename.data] = texture;
		}
	}

	// Load Materials/Textures
	for (unsigned int m = 0; m < model->AiScene->mNumMaterials; m++)
	{
		aiString filename;
		aiMaterial* mat = model->AiScene->mMaterials[m];
		if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &filename) == AI_SUCCESS)
		{
			if (model->Textures.find(filename.C_Str()) == model->Textures.end())
			{
				std::string texPath = directory + "/" + filename.C_Str();
				
				// Convert to wstring
				int len = MultiByteToWideChar(CP_UTF8, 0, texPath.c_str(), -1, nullptr, 0);
				wchar_t* wpath = new wchar_t[len];
				MultiByteToWideChar(CP_UTF8, 0, texPath.c_str(), -1, wpath, len);
				
				ID3D11ShaderResourceView* texture = nullptr;
				ID3D11Resource* resource = nullptr;
				CreateWICTextureFromFile(Direct3D_GetDevice(), Direct3D_GetDeviceContext(), wpath, &resource, &texture);
				delete[] wpath;
				
				if (texture) {
					resource->Release();
					model->Textures[filename.C_Str()] = texture;
				}
			}
		}
	}
	
	if (g_TextureWhiteAni == -1)
		g_TextureWhiteAni = Texture_LoadFromFile(L"resource/texture/white.png");


	// Process Meshes
	model->Meshes.resize(model->AiScene->mNumMeshes);
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		aiMesh* mesh = model->AiScene->mMeshes[m];
		std::vector<Vertex3D_Ani> vertices(mesh->mNumVertices);
		std::vector<unsigned int> indices;

		for (unsigned int v = 0; v < mesh->mNumVertices; v++)
		{
			vertices[v].position = XMFLOAT3(mesh->mVertices[v].x * scale, mesh->mVertices[v].y * scale, mesh->mVertices[v].z * scale);
			vertices[v].normal = XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
			if (mesh->mTextureCoords[0])
				vertices[v].uv = XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
			else
				vertices[v].uv = XMFLOAT2(0, 0);
			vertices[v].color = XMFLOAT4(1, 1, 1, 1);
			
			// Init weights/bones
			vertices[v].weights = XMFLOAT4(0, 0, 0, 0);
			vertices[v].bones = XMUINT4(0, 0, 0, 0);
		}

		// Load Bones for this mesh
		for (unsigned int b = 0; b < mesh->mNumBones; b++)
		{
			aiBone* bone = mesh->mBones[b];
			std::string boneName = bone->mName.C_Str();
			int boneIndex = 0;

			if (model->BoneMapping.find(boneName) == model->BoneMapping.end())
			{
				boneIndex = (int)model->Bones.size();
				Bone newBone;
				newBone.name = boneName;
				newBone.offsetMatrix = ToXMFLOAT4X4(bone->mOffsetMatrix);
				XMStoreFloat4x4(&newBone.localMatrix, XMMatrixIdentity());
				XMStoreFloat4x4(&newBone.globalMatrix, XMMatrixIdentity());
				model->Bones.push_back(newBone);
				model->BoneMapping[boneName] = boneIndex;
			}
			else
			{
				boneIndex = model->BoneMapping[boneName];
			}

			// Weights
			for (unsigned int w = 0; w < bone->mNumWeights; w++)
			{
				unsigned int vertexId = bone->mWeights[w].mVertexId;
				float weight = bone->mWeights[w].mWeight;
				
				// Find empty slot
				if (vertices[vertexId].weights.x == 0.0f) {
					vertices[vertexId].weights.x = weight;
					vertices[vertexId].bones.x = boneIndex;
				} else if (vertices[vertexId].weights.y == 0.0f) {
					vertices[vertexId].weights.y = weight;
					vertices[vertexId].bones.y = boneIndex;
				} else if (vertices[vertexId].weights.z == 0.0f) {
					vertices[vertexId].weights.z = weight;
					vertices[vertexId].bones.z = boneIndex;
				} else if (vertices[vertexId].weights.w == 0.0f) {
					vertices[vertexId].weights.w = weight;
					vertices[vertexId].bones.w = boneIndex;
				}
			}
		}

		// Normalize weights and ensure valid bone assignment
		for (auto& v : vertices)
		{
			float totalWeight = v.weights.x + v.weights.y + v.weights.z + v.weights.w;
			if (totalWeight > 0.0f)
			{
				v.weights.x /= totalWeight;
				v.weights.y /= totalWeight;
				v.weights.z /= totalWeight;
				v.weights.w /= totalWeight;
			}
			else
			{
				// If no weights, assign to first bone (usually root) with weight 1
				v.weights.x = 1.0f;
				v.bones.x = 0; 
			}
		}

		// Indices
		for (unsigned int f = 0; f < mesh->mNumFaces; f++)
		{
			aiFace face = mesh->mFaces[f];
			for (unsigned int i = 0; i < face.mNumIndices; i++)
				indices.push_back(face.mIndices[i]);
		}

		// Create Buffers
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(Vertex3D_Ani) * (UINT)vertices.size();
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertices.data();
		Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->Meshes[m].VertexBuffer);

		bd.ByteWidth = sizeof(unsigned int) * (UINT)indices.size();
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		sd.pSysMem = indices.data();
		Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->Meshes[m].IndexBuffer);

		model->Meshes[m].IndexCount = (unsigned int)indices.size();
		model->Meshes[m].MaterialIndex = mesh->mMaterialIndex;
	}

	// Build Bone Hierarchy
	// We need to traverse the node graph to find parents
	// Assimp nodes have names. If a node name matches a bone name, we can link them.
	// But Assimp nodes are the structure. Bones are just offsets.
	// We need to map the node hierarchy to our bone list.
	// Actually, we should traverse the node tree. If a node corresponds to a bone, we set its parent.
	
	// Helper lambda to traverse
	// We can't use lambda easily with recursion in C++11 without std::function, but let's use a standard recursive function if needed.
	// Or just iterate bones and find parents? No, bones don't store parent info in aiMesh::mBones.
	// We must traverse aiNode.
	
	// Let's create a map of NodeName -> BoneIndex
	// Already have BoneMapping.
	
	struct NodeTraverser {
		MODEL_ANI* model;
		void Traverse(aiNode* node, int parentIndex) {
			std::string nodeName = node->mName.C_Str();
			int boneIndex = -1;
			
			if (model->BoneMapping.count(nodeName)) {
				boneIndex = model->BoneMapping[nodeName];
				model->Bones[boneIndex].parentIndex = parentIndex;
				
				// Initialize local matrix from node transformation (Bind Pose)
				XMMATRIX transform = ToXMMATRIX(node->mTransformation);
				XMStoreFloat4x4(&model->Bones[boneIndex].localMatrix, transform);

				if (parentIndex != -1) {
					model->Bones[parentIndex].children.push_back(boneIndex);
				}
			}
			
			for (unsigned int i = 0; i < node->mNumChildren; i++) {
				Traverse(node->mChildren[i], boneIndex != -1 ? boneIndex : parentIndex);
			}
		}
	};
	NodeTraverser traverser{model};
	traverser.Traverse(model->AiScene->mRootNode, -1);

	// Load Animations
	for (unsigned int i = 0; i < model->AiScene->mNumAnimations; i++)
	{
		aiAnimation* aiAnim = model->AiScene->mAnimations[i];
		Animation anim;
		anim.name = aiAnim->mName.C_Str();
		anim.duration = aiAnim->mDuration;
		anim.ticksPerSecond = aiAnim->mTicksPerSecond != 0 ? aiAnim->mTicksPerSecond : 25.0f;

		for (unsigned int c = 0; c < aiAnim->mNumChannels; c++)
		{
			aiNodeAnim* channel = aiAnim->mChannels[c];
			AnimationChannel ch;
			ch.boneName = channel->mNodeName.C_Str();
			if (model->BoneMapping.count(ch.boneName))
			{
				ch.boneIndex = model->BoneMapping[ch.boneName];
				
				for (unsigned int k = 0; k < channel->mNumPositionKeys; k++) {
					ch.positionKeys.push_back({ channel->mPositionKeys[k].mTime, XMFLOAT3(channel->mPositionKeys[k].mValue.x, channel->mPositionKeys[k].mValue.y, channel->mPositionKeys[k].mValue.z) });
				}
				for (unsigned int k = 0; k < channel->mNumRotationKeys; k++) {
					ch.rotationKeys.push_back({ channel->mRotationKeys[k].mTime, XMFLOAT4(channel->mRotationKeys[k].mValue.x, channel->mRotationKeys[k].mValue.y, channel->mRotationKeys[k].mValue.z, channel->mRotationKeys[k].mValue.w) });
				}
				for (unsigned int k = 0; k < channel->mNumScalingKeys; k++) {
					ch.scalingKeys.push_back({ channel->mScalingKeys[k].mTime, XMFLOAT3(channel->mScalingKeys[k].mValue.x, channel->mScalingKeys[k].mValue.y, channel->mScalingKeys[k].mValue.z) });
				}
				anim.channels.push_back(ch);
			}
		}
		model->Animations.push_back(anim);
	}

	return model;
}

void ModelAni_Release(MODEL_ANI* model)
{
	if (!model) return;
	for (auto& mesh : model->Meshes) {
		if (mesh.VertexBuffer) mesh.VertexBuffer->Release();
		if (mesh.IndexBuffer) mesh.IndexBuffer->Release();
	}
	for (auto& pair : model->Textures) {
		if (pair.second) pair.second->Release();
	}
	aiReleaseImport(model->AiScene);
	delete model;
}

void ModelAni_Update(MODEL_ANI* model, double elapsedTime)
{
	if (model->Animations.empty()) return;

	Animation& anim = model->Animations[model->CurrentAnimationIndex];
	model->CurrentTime += elapsedTime * anim.ticksPerSecond;
	model->CurrentTime = fmod(model->CurrentTime, anim.duration);

	// Reset bone matrices to identity before applying animation?
	// No, we calculate them from scratch.
	
	// For each channel, calculate local transform
	for (const auto& channel : anim.channels)
	{
		Bone& bone = model->Bones[channel.boneIndex];
		
		// Interpolate Position
		XMVECTOR pos = XMVectorSet(0, 0, 0, 1);
		if (!channel.positionKeys.empty()) {
			int idx = FindKeyIndex(model->CurrentTime, channel.positionKeys);
			int nextIdx = (idx + 1) % channel.positionKeys.size();
			double t1 = channel.positionKeys[idx].time;
			double t2 = channel.positionKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((model->CurrentTime - t1) / dt);
			if (dt == 0) factor = 0;
			
			XMVECTOR p1 = XMLoadFloat3(&channel.positionKeys[idx].value);
			XMVECTOR p2 = XMLoadFloat3(&channel.positionKeys[nextIdx].value);
			pos = XMVectorLerp(p1, p2, factor);
		}
		
		// Interpolate Rotation
		XMVECTOR rot = XMVectorSet(0, 0, 0, 1);
		if (!channel.rotationKeys.empty()) {
			int idx = FindKeyIndex(model->CurrentTime, channel.rotationKeys);
			int nextIdx = (idx + 1) % channel.rotationKeys.size();
			double t1 = channel.rotationKeys[idx].time;
			double t2 = channel.rotationKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((model->CurrentTime - t1) / dt);
			if (dt == 0) factor = 0;

			XMVECTOR r1 = XMLoadFloat4(&channel.rotationKeys[idx].value);
			XMVECTOR r2 = XMLoadFloat4(&channel.rotationKeys[nextIdx].value);
			rot = XMQuaternionSlerp(r1, r2, factor);
		}
		
		// Interpolate Scale
		XMVECTOR scale = XMVectorSet(1, 1, 1, 1);
		if (!channel.scalingKeys.empty()) {
			int idx = FindKeyIndex(model->CurrentTime, channel.scalingKeys);
			int nextIdx = (idx + 1) % channel.scalingKeys.size();
			double t1 = channel.scalingKeys[idx].time;
			double t2 = channel.scalingKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((model->CurrentTime - t1) / dt);
			if (dt == 0) factor = 0;

			XMVECTOR s1 = XMLoadFloat3(&channel.scalingKeys[idx].value);
			XMVECTOR s2 = XMLoadFloat3(&channel.scalingKeys[nextIdx].value);
			scale = XMVectorLerp(s1, s2, factor);
		}
		
		XMMATRIX m = XMMatrixScalingFromVector(scale) * XMMatrixRotationQuaternion(rot) * XMMatrixTranslationFromVector(pos);
		XMStoreFloat4x4(&bone.localMatrix, m);
	}
	
	// Update Global Matrices
	// We need to traverse hierarchy again or just iterate if sorted by depth?
	// Bones are not guaranteed to be sorted by depth in the vector.
	// But we can use a recursive helper.
	
	// Wait, we need to update ALL bones, even those not animated (they keep their default local transform? Assimp nodes have default transform).
	// Actually, we should initialize localMatrix from node->mTransformation initially.
	// But for now, let's assume all bones are animated or identity if not.
	// Better: The `localMatrix` in Bone struct should be initialized to node transformation.
	// I did `XMStoreFloat4x4(&newBone.localMatrix, XMMatrixIdentity());` in Load.
	// I should probably fix that to use the node's transformation if no animation.
	// But for now, let's implement the recursive update.
	
	// Find root bones (parentIndex == -1)
	for (int i = 0; i < model->Bones.size(); i++) {
		if (model->Bones[i].parentIndex == -1) {
			// Calculate Global
			// Recursive function
			// We can't use lambda with capture easily for recursion.
			// Let's use a stack or just a separate function?
			// Or just iterate multiple times? No.
			// Let's define a helper function outside or struct.
		}
	}
	
	// Actually, let's just do a simple loop if we can ensure order.
	// But we can't.
	// Let's use a helper struct.
	struct HierarchyUpdater {
		std::vector<Bone>& bones;
		void Update(int index, XMMATRIX parentTransform) {
			XMMATRIX local = XMLoadFloat4x4(&bones[index].localMatrix);
			XMMATRIX global = local * parentTransform;
			XMStoreFloat4x4(&bones[index].globalMatrix, global);
			
			for (int child : bones[index].children) {
				Update(child, global);
			}
		}
	};
	
	HierarchyUpdater updater{model->Bones};
	for (int i = 0; i < model->Bones.size(); i++) {
		if (model->Bones[i].parentIndex == -1) {
			updater.Update(i, XMMatrixIdentity());
		}
	}
}

void ModelAni_Draw(MODEL_ANI* model, const DirectX::XMMATRIX& mtxW)
{
	Shader_3D_Ani_Begin();
	Shader_3D_Ani_SetWorldMatrix(mtxW);
	
	// Prepare Bone Matrices
	std::vector<XMFLOAT4X4> boneMatrices(model->Bones.size());
	for (size_t i = 0; i < model->Bones.size(); i++)
	{
		// Final Matrix = Offset * Global * InverseRoot?
		// Usually: Final = Offset * Global
		// Offset transforms from Mesh Space to Bone Space.
		// Global transforms from Bone Space to Mesh Space (animated).
		// So Vertex * Offset * Global = VertexAnimated.
		
		XMMATRIX offset = XMLoadFloat4x4(&model->Bones[i].offsetMatrix);
		XMMATRIX global = XMLoadFloat4x4(&model->Bones[i].globalMatrix);
		// Also need to apply GlobalInverseTransform of the model root if necessary?
		// Assimp docs say: mOffsetMatrix is the inverse of the global transformation of the bone in bind pose.
		// So Offset * Global should be correct.
		
		// Wait, Assimp's global transform includes the root transform.
		// If we render with mtxW, we are in World Space.
		// The bone matrices should transform from Mesh Space to Mesh Space (deformed).
		
		// Final Matrix = Offset * Global
		// We do NOT multiply by GlobalInverseTransform here because we WANT the Root Node's transform (which includes coordinate conversion) to apply.
		XMMATRIX finalM = offset * global;
		
		XMStoreFloat4x4(&boneMatrices[i], finalM);
	}
	
	Shader_3D_Ani_SetBoneMatrices(boneMatrices.data(), (int)boneMatrices.size());
	
	Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	for (const auto& mesh : model->Meshes)
	{
		// Material
		aiMaterial* mat = model->AiScene->mMaterials[mesh.MaterialIndex];
		aiString texPath;
		if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
		{
			if (model->Textures.count(texPath.C_Str()))
			{
				Direct3D_GetDeviceContext()->PSSetShaderResources(0, 1, &model->Textures[texPath.C_Str()]);
				Shader_3D_Ani_SetColor(XMFLOAT4(1, 1, 1, 1));
			}
		}
		else
		{
			Texture_Set(g_TextureWhiteAni);
			aiColor3D color(1, 1, 1);
			mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			Shader_3D_Ani_SetColor(XMFLOAT4(color.r, color.g, color.b, 1.0f));
		}
		
		UINT stride = sizeof(Vertex3D_Ani);
		UINT offset = 0;
		Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &mesh.VertexBuffer, &stride, &offset);
		Direct3D_GetDeviceContext()->IASetIndexBuffer(mesh.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		
		Direct3D_GetDeviceContext()->DrawIndexed(mesh.IndexCount, 0, 0);
	}
}

void ModelAni_SetAnimation(MODEL_ANI* model, int index)
{
	if (index >= 0 && index < model->Animations.size())
	{
		model->CurrentAnimationIndex = index;
		model->CurrentTime = 0;
	}
}
