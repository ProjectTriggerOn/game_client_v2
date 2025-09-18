#include "cube.h"

#include <DirectXMath.h>

#include "direct3d.h"
#include "shader_3d.h"
#include "key_logger.h"
#include "mouse.h"

using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
	XMFLOAT2 uv; // uv座標
};

namespace {
	constexpr int NUM_VERTEX = 6 * 2 * 3; // 頂点数
	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ

	// 注意！初期化で外部から設定されるもの。Release不要。
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	float g_RotationX = 0.0f;
	float g_RotationY = 0.0f;
	float g_RotationZ = 0.0f;

	double g_AccumulatedTime;

	XMFLOAT3 g_TranslationPosition = { 0.0f, 0.0f, 0.0f };

	XMFLOAT3 g_Scaling = { 1.0f, 1.0f, 1.0f };

	constexpr float MOVE_SPEED = 1.0f;
	constexpr float MOUSE_ROT_SPEED = 0.008f;
	constexpr float WHEEL_ROT_SPEED = 0.004f;
	constexpr float SCALE_SPEED = 0.5f;

	Vertex3D g_CubeVertex[36]
	{
		// 前面 (z=-0.5f) 红色
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		// 后面 (z=0.5f) 绿色
		{{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{-0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		// 左面 (x=-0.5f) 蓝色
		{{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}},
		// 右面 (x=0.5f) 黄色
		{{ 0.5f, 0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f}},
		// 上面 (y=0.5f) 品红色
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f}},
		// 下面 (y=-0.5f) 青色
		{{-0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f}}
	};
}



void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;
	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;// 書き込み不可に設定
	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_CubeVertex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
}

void Cube_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Cube_Draw(XMMATRIX mtxW)
{
	// シェーダーを描画パイプラインに設定
	Shader_3D_Begin();

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	//ワールド座標変換行列を設定
	//XMMATRIX mtxWorld = XMMatrixIdentity();// 単位行列
	//Shader_3D_SetWorldMatrix(mtxWorld);
	XMMATRIX mtxWorld =

		XMMatrixTranslation(
			g_TranslationPosition.x,
			g_TranslationPosition.y,
			g_TranslationPosition.z) *

		XMMatrixRotationX(g_RotationX) *
		XMMatrixRotationY(g_RotationY) *
		XMMatrixRotationZ(g_RotationZ) *

		XMMatrixScaling(
			g_Scaling.x,
			g_Scaling.y * 0.5,
			g_Scaling.z) *
		//turn to pyramid base at y=0
		XMMatrixScaling(
			1.0f, 2.0f, 1.0f
		);
		

	Shader_3D_SetWorldMatrix(mtxW);

	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ポリゴン描画命令発行
	g_pContext->Draw(36, 0);
}

static void Mouse_Control(double elapsed_time)
{
	//Mouse_State mstate;
	//Mouse_GetState(&mstate);
	//if (mstate.positionMode == MOUSE_POSITION_MODE_RELATIVE) {
	//	g_RotationY += mstate.x * MOUSE_ROT_SPEED;
	//	g_RotationX += mstate.y * MOUSE_ROT_SPEED;
	//}
	//if (mstate.scrollWheelValue != 0) {
	//	g_RotationZ += mstate.scrollWheelValue * WHEEL_ROT_SPEED;
	//	Mouse_ResetScrollWheelValue();
	//}
	//if (KeyLogger_IsPressed(KK_A)) g_TranslationPosition.x -= static_cast<float>(MOVE_SPEED * elapsed_time);
	//if (KeyLogger_IsPressed(KK_D)) g_TranslationPosition.x += static_cast<float>(MOVE_SPEED * elapsed_time);
	//if (KeyLogger_IsPressed(KK_LEFTCONTROL)) g_TranslationPosition.y -= static_cast<float>(MOVE_SPEED * elapsed_time);
	//if (KeyLogger_IsPressed(KK_SPACE)) g_TranslationPosition.y += static_cast<float>(MOVE_SPEED * elapsed_time);
	//if (KeyLogger_IsPressed(KK_W)) g_TranslationPosition.z += static_cast<float>(MOVE_SPEED * elapsed_time);
	//if (KeyLogger_IsPressed(KK_S)) g_TranslationPosition.z -= static_cast<float>(MOVE_SPEED * elapsed_time);

	if (KeyLogger_IsPressed(KK_Q)) g_Scaling.y += static_cast<float>(SCALE_SPEED * elapsed_time);
	//	XMFLOAT3(
	//	g_Scaling.x - static_cast<float>(SCALE_SPEED * elapsed_time),
	//	g_Scaling.y - static_cast<float>(SCALE_SPEED * elapsed_time),
	//	g_Scaling.z - static_cast<float>(SCALE_SPEED * elapsed_time)
	//);
	if (KeyLogger_IsPressed(KK_E)) g_Scaling.y -= static_cast<float>(SCALE_SPEED * elapsed_time);
	//	XMFLOAT3(
	//	g_Scaling.x + static_cast<float>(SCALE_SPEED * elapsed_time),
	//	g_Scaling.y + static_cast<float>(SCALE_SPEED * elapsed_time),
	//	g_Scaling.z + static_cast<float>(SCALE_SPEED * elapsed_time)
	//);
}

static void Auto_Control_Demo(double elapsed_time)
{
	//g_RotationX += static_cast<float>(0.5f * elapsed_time);
	g_RotationY += static_cast<float>(0.5f * elapsed_time);
	//g_RotationZ += static_cast<float>(0.5f * elapsed_time);
	//if (g_RotationX > XM_2PI) g_RotationX -= XM_2PI;
	//if (g_RotationY > XM_2PI) g_RotationY -= XM_2PI;
	//if (g_RotationZ > XM_2PI) g_RotationZ -= XM_2PI;
	//g_AccumulatedTime += elapsed_time;
	//g_TranslationPosition.x = static_cast<float>(sin(g_AccumulatedTime)) * 1.5f;
	//g_TranslationPosition.z = static_cast<float>(cos(g_AccumulatedTime)) * 1.5f;
	//g_Scaling = XMFLOAT3(
	//	//g_Scaling.x + static_cast<float>((sin(g_AccumulatedTime)+1.0) *0.5),
	//	1.0,
	//	static_cast<float>((sin(g_AccumulatedTime)+1.0) *0.5),
	//	1.0
	//	//g_Scaling.z + static_cast<float>((sin(g_AccumulatedTime)+1.0) *0.5)
	//);
	
}

void Cube_Update(double elapsed_time)
{

	//Mouse_Control(elapsed_time);
	Auto_Control_Demo(elapsed_time);

}
