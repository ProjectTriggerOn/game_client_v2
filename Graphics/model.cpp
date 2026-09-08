#include <cassert>
#include <cfloat>
#include <vector>
#include "direct3d.h"
#include "texture.h"
#include "model.h"
#include <DirectXMath.h>

#include "shader_3d.h"
#include "shader_3d_unlit.h"
using namespace DirectX;
#include "WICTextureLoader11.h"


struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT3 normal;   // 法線ベクトル
	XMFLOAT4 color;
	XMFLOAT2 uv; // uv座標
};

namespace
{
	int g_TextureWhite = -1;
	XMFLOAT3 ConvertPosition(const aiVector3D& src, bool isBlender)
	{
		if (!isBlender)
			return XMFLOAT3(src.x, src.y, src.z);
		return XMFLOAT3(src.x, -src.z, src.y); // Blender Z 上 → DirectX Y 上
	}

	XMFLOAT3 ConvertNormal(const aiVector3D& src, bool isBlender)
	{
		if (!isBlender)
			return XMFLOAT3(src.x, src.y, src.z);
		return XMFLOAT3(src.x, -src.z, src.y);
	}

	// Assimp keeps an FBX's axis and unit conversion in the node hierarchy, not
	// in the mesh data: a centimetre, Z-up asset arrives as untouched vertices
	// plus a node transform that rotates and scales them. ModelDraw iterates
	// meshes and never walks nodes, so ModelLoad bakes each mesh's accumulated
	// node transform into its vertices. Without it the Low Poly Shooter Pack
	// environment props render lying on their back at 1/100 scale (measured:
	// SM_Lamp_Construction_001 spans 195 units along Z, not Y).
	// Weapons and reticles carry identity node transforms, so for them this is
	// a no-op. A mesh shared by several nodes keeps the last node's transform;
	// none of the project's assets instance a mesh that way.
	void CollectMeshTransforms(const aiNode* node, const aiMatrix4x4& parent,
	                           std::vector<aiMatrix4x4>& out)
	{
		const aiMatrix4x4 world = parent * node->mTransformation;
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			const unsigned int meshIndex = node->mMeshes[i];
			if (meshIndex < out.size()) out[meshIndex] = world;
		}
		for (unsigned int c = 0; c < node->mNumChildren; c++)
			CollectMeshTransforms(node->mChildren[c], world, out);
	}
}



MODEL* ModelLoad(const char* FileName, float scale, bool isBlender,
                 bool bakeNodeTransforms)
{
	MODEL* model = new MODEL;

	// aiProcess_GenBoundingBoxes fills aiMesh::mAABB; without it Assimp leaves the
	// AABB at (0,0,0)/(0,0,0), so ModelGetAABB would return a zero-volume box
	// (breaks the editor's model auto-collider + ray-pick selection — Task 6).
	// static_cast: aiProcess_GenBoundingBoxes == 0x80000000 sets the sign bit, so the
	// OR'd enum expression is a negative int; cast to unsigned for the flags param (C4245).
	model->AiScene = aiImportFile(FileName, static_cast<unsigned int>(aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded | aiProcess_GenBoundingBoxes));

	// Runtime .map props resolve through ModelCatalog_Get, which must survive a
	// missing/corrupt FBX (a shipped map can reference an asset the install
	// lacks). Debug asserts; Release returns nullptr so the caller skips the
	// prop instead of crashing on the null AiScene below.
	if (!model->AiScene) {
		assert(false && "ModelLoad: aiImportFile failed");
		delete model;
		return nullptr;
	}

	model->VertexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];
	model->IndexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];

	// See CollectMeshTransforms: the FBX's axis/unit conversion lives in the
	// node hierarchy, which ModelDraw never walks. Callers that opt in get it
	// folded into the vertices; the rest keep identity, which is the behaviour
	// every asset predating the map system was authored against.
	std::vector<aiMatrix4x4> meshTransform(model->AiScene->mNumMeshes);
	if (bakeNodeTransforms)
		CollectMeshTransforms(model->AiScene->mRootNode, aiMatrix4x4(), meshTransform);

	aiVector3D bakedMin( FLT_MAX,  FLT_MAX,  FLT_MAX);
	aiVector3D bakedMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);


	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		aiMesh* mesh = model->AiScene->mMeshes[m];


		{
			Vertex3D* vertex = new Vertex3D[mesh->mNumVertices];

			// Neither channel is guaranteed. aiProcess_SortByPType can hand back a
			// sub-mesh with no UV set, and an asset may ship without normals; both
			// used to be dereferenced blind, which turned a missing UV channel into
			// an access violation the moment a map referenced such a model.
			const aiVector3D* uv0     = mesh->mTextureCoords[0];
			const aiVector3D* normals = mesh->mNormals;

			for (unsigned int v = 0; v < mesh->mNumVertices; v++)
			{
				XMFLOAT3 pos = ConvertPosition(meshTransform[m] * mesh->mVertices[v], isBlender);
				vertex[v].position = XMFLOAT3(pos.x * scale, pos.y * scale, pos.z * scale);
				// Rotation/scale part only. The node transforms in this project are
				// rotation plus uniform scale, so re-normalising is equivalent to the
				// inverse-transpose a non-uniform scale would require.
				aiVector3D rawNormal = normals ? aiMatrix3x3(meshTransform[m]) * normals[v]
				                               : aiVector3D(0.0f, 1.0f, 0.0f);
				if (rawNormal.SquareLength() > 0.0f) rawNormal.Normalize();
				XMFLOAT3 normal = ConvertNormal(rawNormal, isBlender);
				vertex[v].normal = normal;
				vertex[v].uv = uv0 ? XMFLOAT2(uv0[v].x, uv0[v].y) : XMFLOAT2(0.0f, 0.0f);
				vertex[v].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

				const XMFLOAT3& bp = vertex[v].position;
				if (bp.x < bakedMin.x) bakedMin.x = bp.x;
				if (bp.y < bakedMin.y) bakedMin.y = bp.y;
				if (bp.z < bakedMin.z) bakedMin.z = bp.z;
				if (bp.x > bakedMax.x) bakedMax.x = bp.x;
				if (bp.y > bakedMax.y) bakedMax.y = bp.y;
				if (bp.z > bakedMax.z) bakedMax.z = bp.z;
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(Vertex3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = 0;
			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = vertex;

			Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->VertexBuffer[m]);

			delete[] vertex;
		}



		{
			// 统计所有面三角形总数（多边形面拆分为多个三角形）
			unsigned int numTriangles = 0;
			for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
				const aiFace* face = &mesh->mFaces[f];
				if (face->mNumIndices >= 3) {
					numTriangles += (face->mNumIndices - 2);
				}
			}
			unsigned int* index = new unsigned int[numTriangles * 3];
			unsigned int idx = 0;
			for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
				const aiFace* face = &mesh->mFaces[f];
				if (face->mNumIndices < 3) {
					// 跳过无效面
					continue;
				}
				// 扇形三角化
				for (unsigned int t = 0; t < face->mNumIndices - 2; t++) {
					index[idx * 3 + 0] = face->mIndices[0];
					index[idx * 3 + 1] = face->mIndices[t + 1];
					index[idx * 3 + 2] = face->mIndices[t + 2];
					idx++;
				}
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(unsigned int) * numTriangles * 3;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = index;

			Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);

			delete[] index;
		}

	}

	// A model with no vertices leaves the sentinels untouched; collapse it to
	// an empty box rather than handing ModelGetAABB an inverted one.
	if (bakedMin.x > bakedMax.x) { bakedMin = aiVector3D(0.0f, 0.0f, 0.0f); bakedMax = bakedMin; }
	model->LocalMin = XMFLOAT3(bakedMin.x, bakedMin.y, bakedMin.z);
	model->LocalMax = XMFLOAT3(bakedMax.x, bakedMax.y, bakedMax.z);

	g_TextureWhite = Texture_LoadFromFile(L"resource/texture/white.png");

	//FBXに埋め込まれたテクスチャの読み込み
	for (unsigned int i = 0; i < model->AiScene->mNumTextures; i++)
	{
		aiTexture* aitexture = model->AiScene->mTextures[i];

		ID3D11ShaderResourceView* texture;
		ID3D11Resource* resource;

		CreateWICTextureFromMemory(
			Direct3D_GetDevice(),
			Direct3D_GetDeviceContext(),
			(const uint8_t*)aitexture->pcData,
			(size_t)aitexture->mWidth,
			&resource,
			&texture);

		assert(texture);

		resource->Release();// リソースは不要なので解放

		model->Texture[aitexture->mFilename.data] = texture;
	}

	//ディレクトリパスの取得
	const std::string modelPath(FileName);


	// 最後のスラッシュの位置を検索
	size_t pos = modelPath.find_last_of("/\\");
	std::string directory;


	if (pos != std::string::npos)
	{
		directory = modelPath.substr(0, pos);// スラッシュまでを抽出
	}
	else
	{
		directory = "";// スラッシュが見つからなかった場合は空文字列
	}

	//テクスチャがFBXとは別に用意されている場合の読み込み
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		aiString filename;
		aiMaterial* aiMaterial = model->AiScene->mMaterials[model->AiScene->mMeshes[m]->mMaterialIndex];
		aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &filename);

		if (filename.length == 0)
		{
			continue;
		}
		if (model->Texture.count(filename.C_Str()))
		{
			continue;
		}

		ID3D11ShaderResourceView* texture;
		ID3D11Resource* resource;

		std::string texFileName = directory + "/" + filename.C_Str();
		int len = MultiByteToWideChar(
			CP_UTF8,
			0,
			texFileName.c_str(),
			-1,
			nullptr,
			0);
		wchar_t* pWideFileName = new wchar_t[len];
		MultiByteToWideChar(
			CP_UTF8,
			0,
			texFileName.c_str(),
			-1,
			pWideFileName,
			len);
		texture = nullptr;
		resource = nullptr;
		const HRESULT hr = CreateWICTextureFromFile(
			Direct3D_GetDevice(),
			Direct3D_GetDeviceContext(),
			pWideFileName,
			&resource,
			&texture);
		delete[] pWideFileName;
		// A map may reference an asset whose sidecar texture is missing. Debug
		// asserts; Release must not cache the uninitialised pointer the failed
		// call leaves behind — ModelDraw would bind it. Skipping the entry makes
		// the mesh fall back to the white texture plus its material colour.
		if (FAILED(hr) || !texture)
		{
			assert(false && "ModelLoad: sidecar texture failed to load");
			if (resource) resource->Release();
			continue;
		}
		resource->Release();// リソースは不要なので解放
		model->Texture[filename.C_Str()] = texture;

	}

	return model;
}


void ModelRelease(MODEL* model)
{
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		model->VertexBuffer[m]->Release();
		model->IndexBuffer[m]->Release();
	}

	delete[] model->VertexBuffer;
	delete[] model->IndexBuffer;


	for (std::pair<const std::string, ID3D11ShaderResourceView*> pair : model->Texture)
	{
		pair.second->Release();
	}


	aiReleaseImport(model->AiScene);


	delete model;
}

void ModelDraw(MODEL* model, const DirectX::XMMATRIX& mtxW)
{
	// シェーダーを描画パイプラインに設定
	Shader_3D_Begin();
	// ワールド行列設定
	Shader_3D_SetWorldMatrix(mtxW);
	// プリミティブトポロジ設定
	Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		aiString texture;
		aiMaterial* aiMaterial = model->AiScene->mMaterials[model->AiScene->mMeshes[m]->mMaterialIndex];
		aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);

		if (texture.length != 0)
		{
			Direct3D_GetDeviceContext()->PSSetShaderResources(0, 1, &model->Texture[texture.data]);
			Shader_3D_SetColor({ 1.0f,1.0f,1.0f,1.0f });
		}
		else
		{
			Texture_Set(g_TextureWhite);
			aiColor3D diffuse;
			aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			Shader_3D_SetColor(XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f));
		}

		aiColor3D diffuse;
		aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		Shader_3D_SetColor(XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f));

		// 頂点バッファを描画パイプラインに設定
		UINT stride = sizeof(Vertex3D);
		UINT offset = 0;
		Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &model->VertexBuffer[m], &stride, &offset);
		Direct3D_GetDeviceContext()->IASetIndexBuffer(model->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		Direct3D_GetDeviceContext()->DrawIndexed(
			model->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}
}




void ModelDrawUnlit(MODEL* model, const DirectX::XMMATRIX& mtxW)
{
	// シェーダーを描画パイプラインに設定
	Shader_3DUnlit_Begin();
	// ワールド行列設定
	Shader_3DUnlit_SetWorldMatrix(mtxW);
	// プリミティブトポロジ設定
	Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		aiString texture;
		aiMaterial* aiMaterial = model->AiScene->mMaterials[model->AiScene->mMeshes[m]->mMaterialIndex];
		aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);

		if (texture.length != 0)
		{
			Direct3D_GetDeviceContext()->PSSetShaderResources(0, 1, &model->Texture[texture.data]);
			Shader_3DUnlit_SetColor({ 1.0f,1.0f,1.0f,1.0f });
		}
		else
		{
			Texture_Set(g_TextureWhite);
			aiColor3D diffuse;
			aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			Shader_3DUnlit_SetColor(XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f));
		}

		aiColor3D diffuse;
		aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		Shader_3DUnlit_SetColor(XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f));

		// 頂点バッファを描画パイプラインに設定
		UINT stride = sizeof(Vertex3D);
		UINT offset = 0;
		Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &model->VertexBuffer[m], &stride, &offset);
		Direct3D_GetDeviceContext()->IASetIndexBuffer(model->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		Direct3D_GetDeviceContext()->DrawIndexed(
			model->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}
}

AABB ModelGetAABB(MODEL* model, const DirectX::XMFLOAT3& position)
{
	// MODEL::LocalMin/Max, not aiMesh::mAABB: ModelLoad bakes the node
	// transform, the load-time scale and the axis swap into the vertices it
	// uploads, none of which mAABB knows about. Reading mAABB here used to
	// hand the editor a collider in the raw mesh's frame — for a centimetre,
	// Z-up prop that is both the wrong size and the wrong axis.
	AABB aabb;
	aabb.min.x = model->LocalMin.x + position.x;
	aabb.min.y = model->LocalMin.y + position.y;
	aabb.min.z = model->LocalMin.z + position.z;
	aabb.max.x = model->LocalMax.x + position.x;
	aabb.max.y = model->LocalMax.y + position.y;
	aabb.max.z = model->LocalMax.z + position.z;
	return aabb;
}
