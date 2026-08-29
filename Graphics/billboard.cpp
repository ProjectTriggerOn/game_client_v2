#include "billboard.h"
#include "shader_billboard.h"
#include "texture.h"
#include "direct3d.h"
#include "debug_log.h"

using namespace DirectX;

namespace
{
	constexpr int NUM_VERTEX = 4;
	struct Vertex3D
	{
		XMFLOAT3 position; // local position
		XMFLOAT4 color;
		XMFLOAT2 uv;
	};

	ID3D11Buffer* g_pVertexBuffer = nullptr;
	XMFLOAT4X4 g_CameraView{}; // injected each frame by Billboard_SetCamera
}

void Billboard_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext*)
{
	// Game_Initialize re-enters on every round, so the vertex buffer would leak
	// without releasing the previous one first.
	SAFE_RELEASE(g_pVertexBuffer);

	const Vertex3D vertices[]
	{
		{ { -0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },  // top-left
		{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },  // top-right
		{ { -0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },  // bottom-left
		{ {  0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },  // bottom-right
	};

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertices;
	pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
}

void Billboard_Finalize(void)
{
	// Shader_Billboard_Finalize is owned by main.cpp's process-wide sequence
	// (like every other shader module); this per-scene teardown only owns the
	// quad vertex buffer.
	SAFE_RELEASE(g_pVertexBuffer);
}

void Billboard_SetCamera(const XMFLOAT4X4& view)
{
	g_CameraView = view;
}

void Billboard_Draw(int texID, const XMFLOAT3& position, const XMFLOAT2& scale,
	const XMFLOAT2& pivot, const XMFLOAT4& color)
{
	Shader_Billboard_SetUVParameter({ { 1.0f, 1.0f }, { 0.0f, 0.0f } });
	Shader_Billboard_Begin();
	Shader_Billboard_SetColor(color);

	Texture_Set(texID);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	Direct3D_GetDeviceContext()->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);

	// Camera basis: strip the view's translation and transpose — rows of the
	// result are the camera's (right, up, forward) in world space (LPSP trick,
	// decoupled from any specific camera module).
	XMFLOAT4X4 camView = g_CameraView;
	camView._41 = camView._42 = camView._43 = 0.0f;
	const XMMATRIX cameraBasis = XMMatrixTranspose(XMLoadFloat4x4(&camView));

	const XMMATRIX pivotOffset = XMMatrixTranslation(-pivot.x, -pivot.y, 0.0f);
	const XMMATRIX mtxs = XMMatrixScaling(scale.x, scale.y, 1.0f);
	const XMMATRIX mtxt = XMMatrixTranslation(position.x + pivot.x, position.y + pivot.y, position.z);

	Shader_Billboard_SetWorldMatrix(pivotOffset * mtxs * cameraBasis * mtxt);

	Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Direct3D_GetDeviceContext()->Draw(NUM_VERTEX, 0);

	// TEMP DIAGNOSTIC (matrix-path investigation): log the working path's
	// world matrix once.
	static bool s_loggedCamera = false;
	if (!s_loggedCamera)
	{
		s_loggedCamera = true;
		const XMMATRIX m = pivotOffset * mtxs * cameraBasis * mtxt;
		DebugLog_Printf("impact",
			"CAM world rows: [%.3f %.3f %.3f %.3f][%.3f %.3f %.3f %.3f][%.3f %.3f %.3f %.3f][%.3f %.3f %.3f %.3f]",
			m.r[0].m128_f32[0], m.r[0].m128_f32[1], m.r[0].m128_f32[2], m.r[0].m128_f32[3],
			m.r[1].m128_f32[0], m.r[1].m128_f32[1], m.r[1].m128_f32[2], m.r[1].m128_f32[3],
			m.r[2].m128_f32[0], m.r[2].m128_f32[1], m.r[2].m128_f32[2], m.r[2].m128_f32[3],
			m.r[3].m128_f32[0], m.r[3].m128_f32[1], m.r[3].m128_f32[2], m.r[3].m128_f32[3]);
	}
}

void Billboard_DrawWorld(int texID, const XMMATRIX& world, const XMFLOAT4& color)
{
	Shader_Billboard_SetUVParameter({ { 1.0f, 1.0f }, { 0.0f, 0.0f } });
	Shader_Billboard_Begin();
	Shader_Billboard_SetColor(color);

	Texture_Set(texID);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	Direct3D_GetDeviceContext()->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);

	Shader_Billboard_SetWorldMatrix(world);

	Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Direct3D_GetDeviceContext()->Draw(NUM_VERTEX, 0);

	// TEMP DIAGNOSTIC (matrix-path investigation): log the decal path's
	// world matrix once.
	static bool s_loggedWorld = false;
	if (!s_loggedWorld)
	{
		s_loggedWorld = true;
		DebugLog_Printf("impact",
			"DECAL world rows: [%.3f %.3f %.3f %.3f][%.3f %.3f %.3f %.3f][%.3f %.3f %.3f %.3f][%.3f %.3f %.3f %.3f]",
			world.r[0].m128_f32[0], world.r[0].m128_f32[1], world.r[0].m128_f32[2], world.r[0].m128_f32[3],
			world.r[1].m128_f32[0], world.r[1].m128_f32[1], world.r[1].m128_f32[2], world.r[1].m128_f32[3],
			world.r[2].m128_f32[0], world.r[2].m128_f32[1], world.r[2].m128_f32[2], world.r[2].m128_f32[3],
			world.r[3].m128_f32[0], world.r[3].m128_f32[1], world.r[3].m128_f32[2], world.r[3].m128_f32[3]);
	}
}
