#include "mesh_field.h"

using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
	XMFLOAT2 uv; // uv座標
};

namespace 
{
	Vertex3D g_MeshFieldVertex[10000];
}

void MeshField_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
}

void MeshField_Finalize()
{
}
