#include <assert.h>
#include "direct3d.h"
#include "texture.h"
#include "model.h"
#include <DirectXMath.h>

#include "shader_3d.h"
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
}

MODEL* ModelLoad(const char* FileName,float scale)
{
	MODEL* model = new MODEL;


	//const std::string modelPath(FileName);

	model->AiScene = aiImportFile(FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);
	assert(model->AiScene);

	model->VertexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];
	model->IndexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];


	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		aiMesh* mesh = model->AiScene->mMeshes[m];


		{
			Vertex3D* vertex = new Vertex3D[mesh->mNumVertices];

			for (unsigned int v = 0; v < mesh->mNumVertices; v++)
			{
				vertex[v].position = XMFLOAT3(
					mesh->mVertices[v].x * scale,
					mesh->mVertices[v].y * scale,
					mesh->mVertices[v].z * scale);
				vertex[v].uv = XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
				vertex[v].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				vertex[v].normal = XMFLOAT3(
					mesh->mNormals[v].x * scale,
					mesh->mNormals[v].y * scale,
					mesh->mNormals[v].z * scale);
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(Vertex3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			//bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			bd.CPUAccessFlags = 0;
			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = vertex;

			Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->VertexBuffer[m]);

			delete[] vertex;
		}



		{
			unsigned int* index = new unsigned int[mesh->mNumFaces * 3];

			for (unsigned int f = 0; f < mesh->mNumFaces; f++)
			{
				const aiFace* face = &mesh->mFaces[f];

				assert(face->mNumIndices == 3);

				index[f * 3 + 0] = face->mIndices[0];
				index[f * 3 + 1] = face->mIndices[1];
				index[f * 3 + 2] = face->mIndices[2];
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(unsigned int) * mesh->mNumFaces * 3;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = index;

			Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);

			delete[] index;
		}

	}

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
			(const uint8_t*) aitexture->pcData,
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
		if (model ->Texture.count(filename.C_Str()))
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
		CreateWICTextureFromFile(
			Direct3D_GetDevice(),
			Direct3D_GetDeviceContext(),
			pWideFileName,
			&resource,
			&texture);
		delete[] pWideFileName;
		assert(texture);
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
	Shader_3D_SetColor({ 1.0f,1.0f,1.0f,1.0f });
	//Texture_Set();
	Shader_3D_SetWorldMatrix(mtxW);
	// プリミティブトポロジ設定
	Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (unsigned int m = 0;m <model->AiScene->mNumMeshes;m++)
	{


		aiString texture;
		aiMaterial* aiMaterial = model->AiScene->mMaterials[model->AiScene->mMeshes[m]->mMaterialIndex];
		aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);

		if (texture.length != 0)
		//if (texture != aiString(""))
		{
			Direct3D_GetDeviceContext()->PSSetShaderResources(0, 1, &model->Texture[texture.data]);
		}else
		{
			Texture_Set(g_TextureWhite);
		}

		//aiMaterial* aiMaterial = model->AiScene->mMaterials[model->AiScene->mMeshes[m]->mMaterialIndex];
		aiColor3D diffuse;
		aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		Shader_3D_SetColor(XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f));

		// 頂点バッファを描画パイプラインに設定
		UINT stride = sizeof(Vertex3D);
		UINT offset = 0;
		Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &model->VertexBuffer[m], &stride, &offset);
		Direct3D_GetDeviceContext()->IASetIndexBuffer(model->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		Direct3D_GetDeviceContext()->DrawIndexed(
			model->AiScene->mMeshes[m]->mNumFaces*3, 0, 0);
	}
}