#include "mesh_field.h"

#include "camera.h"
#include "direct3d.h"
#include "shader_field.h"
#include "texture.h"

using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT3 normal;   // 法線ベクトル
	XMFLOAT4 color;
	XMFLOAT2 uv; // uv座標
};


namespace 
{
	constexpr float FIELD_MESH_SIZE = 1.0f; // メッシュの1マスの大きさ
	constexpr int FIELD_MESH_H_COUNT = 50; // 横方向の分割数
	constexpr int FIELD_MESH_V_COUNT = 50; // 縦方向の分割数
	constexpr int FIELD_MESH_H_VERTEX_COUNT = FIELD_MESH_H_COUNT + 1;// 横方向の頂点数
	constexpr int FIELD_MESH_V_VERTEX_COUNT = FIELD_MESH_V_COUNT + 1;// 縦方向の頂点数

	constexpr int NUM_VERTEX = FIELD_MESH_H_VERTEX_COUNT * FIELD_MESH_V_VERTEX_COUNT ; // 頂点数
	constexpr int NUM_INDEX = FIELD_MESH_H_COUNT * FIELD_MESH_V_COUNT * 6;//
	Vertex3D g_MeshFieldVertex[NUM_VERTEX];
	unsigned short g_MeshFieldVertexIndex[NUM_INDEX];
	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
	ID3D11Buffer* g_pIndexBuffer = nullptr; // 頂点バッファ

	// 注意！初期化で外部から設定されるもの。Release不要。
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	int g_MeshFieldTexId0 = -1; // テクスチャID
	int g_MeshFieldTexId1 = -1; // テクスチャID
}

void MeshField_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pContext = pContext;
	g_pDevice = pDevice;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;// 書き込み不可に設定
	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	for (int z = 0; z < FIELD_MESH_V_VERTEX_COUNT;z++)
	{
		for (int x = 0; x< FIELD_MESH_H_VERTEX_COUNT;x++)
		{
			//横+横の最大数　*縦;
			int index = x + z * FIELD_MESH_H_VERTEX_COUNT;
			g_MeshFieldVertex[index].position = {
				 x * FIELD_MESH_SIZE,
				0.0f,
				 z * FIELD_MESH_SIZE
			};

			g_MeshFieldVertex[index].normal = { 0.0f,1.0f,0.0f };
			g_MeshFieldVertex[index].color = { 0.0f,1.0f,0.0f,1.0f };
			g_MeshFieldVertex[index].uv = {x * 1.0f,z * 1.0f};
		}

	}

	// インデックスバッファの設定
	int index;
	for (int z = 0; z < FIELD_MESH_V_COUNT; z++)
	{
		for (int x = 0; x < FIELD_MESH_H_COUNT; x++)
		{
			index = (x + z * FIELD_MESH_H_COUNT) * 6;
			g_MeshFieldVertexIndex[index + 0] = x + (z + 0) * FIELD_MESH_H_VERTEX_COUNT;      //0 1    5
			g_MeshFieldVertexIndex[index + 1] = x + (z + 1) * FIELD_MESH_H_VERTEX_COUNT + 1;  //5 6    10
			g_MeshFieldVertexIndex[index + 2] = g_MeshFieldVertexIndex[index + 0] + 1;		  //1 2    6
			g_MeshFieldVertexIndex[index + 3] = g_MeshFieldVertexIndex[index + 0];			  //0 1    5
			g_MeshFieldVertexIndex[index + 4] = g_MeshFieldVertexIndex[index + 1] - 1;		  //4 5    9
			g_MeshFieldVertexIndex[index + 5] = g_MeshFieldVertexIndex[index + 1];			  //5 6    10
			index += 6;
		}
	}

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_MeshFieldVertex;
	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);

	bd.Usage = D3D11_USAGE_DEFAULT;// 書き込み不可に設定
	bd.ByteWidth = sizeof(unsigned short) * NUM_INDEX;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	sd.pSysMem = g_MeshFieldVertexIndex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pIndexBuffer);

	g_MeshFieldTexId0 = Texture_LoadFromFile(L"resource/texture/Grass.png");
	g_MeshFieldTexId1 = Texture_LoadFromFile(L"resource/texture/stone_001.jpg");

}

void MeshField_Finalize()
{
	SAFE_RELEASE(g_pVertexBuffer)
}

void MeshField_Draw(const DirectX::XMMATRIX& mtxW)
{
	// シェーダーを描画パイプラインに設定
	Shader_Field_Begin();
	Texture_Set(g_MeshFieldTexId0,0);
	Texture_Set(g_MeshFieldTexId1,1);

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);


	float offset_x = FIELD_MESH_H_COUNT * FIELD_MESH_SIZE * 0.5f;
	float offset_z = FIELD_MESH_V_COUNT * FIELD_MESH_SIZE * 0.5f;

	//Shader_3D_SetWorldMatrix(mtxW);

	Shader_Field_SetWorldMatrix(XMMatrixTranslation(-offset_x, 0.0f, -offset_z));
	//Shader_Field_SetViewMatrix(XMLoadFloat4x4(&Camera_GetViewMatrix()));
	//Shader_Field_SetProjectMatrix(XMLoadFloat4x4(&Camera_GetPerspectiveMatrix()));
	Shader_Field_SetColor({ 1.0f,1.0f,1.0f,1.0f });

	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ポリゴン描画命令発行
	g_pContext->DrawIndexed(NUM_INDEX, 0, 0);
}
