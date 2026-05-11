#include "cube.h"

#include <DirectXMath.h>

#include "direct3d.h"
#include "shader_3d.h"
#include "key_logger.h"
#include "mouse.h"
#include "texture.h"

using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT4 color;
	XMFLOAT2 uv;
};

namespace {
	int g_CubeTexId = -1;

	constexpr int NUM_VERTEX = 24;
	constexpr int NUM_INDEX = 36;
	ID3D11Buffer* g_pVertexBufferAtlas = nullptr;   // Atlas UV vertex buffer
	ID3D11Buffer* g_pVertexBufferPerFace = nullptr; // Per-face UV vertex buffer
	ID3D11Buffer* g_pIndexBuffer = nullptr;
	CubeUVMode g_UVMode = CUBE_UV_ATLAS;

	// NOTE: set from outside in Initialize(); we don't own these (no Release needed).
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	float g_RotationX = 0.0f;
	float g_RotationY = 0.0f;
	float g_RotationZ = 0.0f;

	XMFLOAT3 g_TranslationPosition = { 0.0f, 0.0f, 0.0f };

	XMFLOAT3 g_Scaling = { 1.0f, 1.0f, 1.0f };

	constexpr float SCALE_SPEED = 0.5f;

	Vertex3D g_CubeVertex[]
	{
		// Front face (z=-0.5)
		{{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,-1.0f}, { 1.0f,1.0f,1.0f,1.0f }, {0.0f, 0.5f}},
		{{-0.5f, 0.5f,-0.5f},{0.0f,0.0f,-1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.0f}},
		{{ 0.5f, 0.5f,-0.5f},{0.0f,0.0f,-1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.25f, 0.0f}},
		{{ 0.5f,-0.5f,-0.5f},{0.0f,0.0f,-1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.25f, 0.5f}},
		// Back face (z=0.5)
		{{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.5f, 0.0f}},
		{{-0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.5f, 0.5f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.25f, 0.5f}},
		{{ 0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.25f, 0.0f}},
		// Left face (x=-0.5)
		{{-0.5f, 0.5f, 0.5f},{-1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.75f, 0.0f}},
		{{-0.5f, 0.5f,-0.5f},{-1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.75f, 0.5f}},
		{{-0.5f,-0.5f,-0.5f},{-1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.5f, 0.5f}},
		{{-0.5f,-0.5f, 0.5f},{-1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.5f, 0.0f}},
		// Right face (x=0.5)
		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.25f, 0.5f}},
		{{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.25f, 1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.0f, 1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.0f, 0.5f}},
		// Top face (y=0.5)
		{{-0.5f, 0.5f,-0.5f},{0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.5f,  0.5f}},
		{{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.5f,  1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.25f,1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.25f,0.5f}},
		// Bottom face (y=-0.5)
		{{-0.5f,-0.5f, 0.5f},{0.0f,-1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.5f,  1.0f}},
		{{-0.5f,-0.5f,-0.5f},{0.0f,-1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.5f,  0.5f}},
		{{ 0.5f,-0.5f,-0.5f},{0.0f,-1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.75f, 0.5f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,-1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f},{0.75f, 1.0f}},
	};
	// Per-face UV: every face maps full [0,0]-[1,1]
	Vertex3D g_CubeVertexPerFace[]
	{
		// Front face (z=-0.5)
		{{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,-1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 1.0f}},
		{{-0.5f, 0.5f,-0.5f},{0.0f,0.0f,-1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.0f}},
		{{ 0.5f, 0.5f,-0.5f},{0.0f,0.0f,-1.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 0.0f}},
		{{ 0.5f,-0.5f,-0.5f},{0.0f,0.0f,-1.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 1.0f}},
		// Back face (z=0.5)
		{{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 0.0f}},
		{{-0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.0f}},
		// Left face (x=-0.5)
		{{-0.5f, 0.5f, 0.5f},{-1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.0f}},
		{{-0.5f, 0.5f,-0.5f},{-1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 0.0f}},
		{{-0.5f,-0.5f,-0.5f},{-1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 1.0f}},
		{{-0.5f,-0.5f, 0.5f},{-1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 1.0f}},
		// Right face (x=0.5)
		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.0f}},
		{{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 0.0f}},
		{{ 0.5f,-0.5f, 0.5f},{1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 1.0f}},
		// Top face (y=0.5)
		{{-0.5f, 0.5f,-0.5f},{0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.0f}},
		{{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 0.0f}},
		// Bottom face (y=-0.5)
		{{-0.5f,-0.5f, 0.5f},{0.0f,-1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.0f}},
		{{-0.5f,-0.5f,-0.5f},{0.0f,-1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {0.0f, 1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{0.0f,-1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,-1.0f,0.0f}, {1.0f,1.0f,1.0f,1.0f}, {1.0f, 0.0f}},
	};

	unsigned short g_CubeVertexIndex[36]
	{
		 0, 1, 2,  0, 2, 3,    // Front
		 4, 5, 6,  4, 6, 7,    // Back
		 8, 9,10,  8,10,11,    // Left
		12,13,14, 12,14,15,    // Right
		16,17,18, 16,18,19,    // Top
		20,21,22, 20,22,23,    // Bottom
	};
}



void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pDevice = pDevice;
	g_pContext = pContext;

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_CubeVertex;
	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBufferAtlas);

	sd.pSysMem = g_CubeVertexPerFace;
	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBufferPerFace);

	bd.ByteWidth = sizeof(unsigned short) * NUM_INDEX;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	sd.pSysMem = g_CubeVertexIndex;
	g_pDevice->CreateBuffer(&bd, &sd, &g_pIndexBuffer);

	g_CubeTexId = Texture_LoadFromFile(L"resource/texture/cube_tex.png");
}

void Cube_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBufferAtlas);
	SAFE_RELEASE(g_pVertexBufferPerFace);
	SAFE_RELEASE(g_pIndexBuffer);
}

void Cube_Draw(int texID, const XMMATRIX& mtxW)
{
	Shader_3D_Begin();
	Shader_3D_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

	Texture_Set(texID);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	ID3D11Buffer* pVB = (g_UVMode == CUBE_UV_PER_FACE) ? g_pVertexBufferPerFace : g_pVertexBufferAtlas;
	g_pContext->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);
	g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	Shader_3D_SetWorldMatrix(mtxW);

	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_pContext->DrawIndexed(NUM_INDEX, 0, 0);
}


void Cube_Update(double)
{

}

void Cube_SetUVMode(CubeUVMode mode)
{
	g_UVMode = mode;
}

CubeUVMode Cube_GetUVMode()
{
	return g_UVMode;
}

AABB Cube_GetAABB(const DirectX::XMFLOAT3& cubePos)
{
	return {
		{cubePos.x - 0.5f, cubePos.y - 0.5f, cubePos.z - 0.5f},
		{cubePos.x + 0.5f, cubePos.y + 0.5f, cubePos.z + 0.5f}
	};
}
