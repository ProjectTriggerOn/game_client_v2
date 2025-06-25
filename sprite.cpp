#include <d3d11.h>
#include <DirectXMath.h>
#include "DirectXTex.h"
using namespace DirectX;
#include "direct3d.h"
#include "shader.h"
#include "debug_ostream.h"
#include "sprite.h"
#include "texture.h"


static constexpr int NUM_VERTEX = 4; // 頂点数



static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
static ID3D11ShaderResourceView* g_pTexture = nullptr;

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;


// 頂点構造体
struct Vertex
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
	XMFLOAT2 uv;
};


void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストのチェック
	if (!pDevice || !pContext) {
		hal::dout << "Sprite_Initialize() : 与えられたデバイスかコンテキストが不正です" << std::endl;
		return;
	}

	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	g_pDevice->CreateBuffer(&bd, NULL, &g_pVertexBuffer);


}

void Sprite_Finalize(void)
{
	SAFE_RELEASE(g_pTexture);
	SAFE_RELEASE(g_pVertexBuffer);
}

void Sprite_Begin()
{
	const float SCREEN_WIDTH = (float)Direct3D_GetBackBufferWidth();
	const float SCREEN_HEIGHT = (float)Direct3D_GetBackBufferHeight();

	Shader_SetProjectMatrix(XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f));
}

void Sprite_Draw(int texid, float sx, float sy, const DirectX::XMFLOAT4& color)
{
  // シェーダーを描画パイプラインに設定  
	Shader_Begin();

	// 頂点バッファをロックする  
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得  
	Vertex* v = (Vertex*)msr.pData;

	unsigned int dw = Texture_GetWidth(texid);
	unsigned int dh = Texture_GetHeight(texid);


	// 画面の左上から右下に向かう線分を描画する  
	v[0].position = { sx     ,sy , 0.0f };
	v[1].position = { sx + dw,sy , 0.0f };
	v[2].position = { sx     ,sy + dh, 0.0f };
	v[3].position = { sx + dw,sy + dh, 0.0f };

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	v[0].uv = { 0    , 0 };  // 左上のUV座標
	v[1].uv = { 1.0f , 0.0f };  // 右上のUV座標
	v[2].uv = { 0.0f , 1.0f };  // 左下のUV座標
	v[3].uv = { 1.0f , 1.0f };  // 右下のUV座標

	//a -= 0.02f; // UV座標をアニメーションさせるための変数

	// 頂点バッファのロックを解除  
	g_pContext->Unmap(g_pVertexBuffer, 0);
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定  
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// 頂点シェーダーに変換行列を設定  
	//Shader_SetMatrix(XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f));

	// プリミティブトポロジ設定  
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Texture_Set(texid); // テクスチャを設定

	// ポリゴン描画命令発行  
	g_pContext->Draw(NUM_VERTEX, 0);

}

void Sprite_Draw(int texid, float sx, float sy, int pixx, int pixy, int pixw, int pixh,
	const DirectX::XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定  
	Shader_Begin();

	// 頂点バッファをロックする  
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得  
	Vertex* v = (Vertex*)msr.pData;

	// 画面の左上から右下に向かう線分を描画する  
	v[0].position = { sx       ,sy , 0.0f };
	v[1].position = { sx + pixw,sy , 0.0f };
	v[2].position = { sx       ,sy + pixh, 0.0f };
	v[3].position = { sx + pixw,sy + pixh, 0.0f };

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	float tw = (float)Texture_GetWidth(texid);
	float th = (float)Texture_GetHeight(texid);


	float u0 = pixx / tw;
	float v0 = pixy / th;
	float u1 = (pixx + pixw) / tw;
	float v1 = (pixy + pixh) / th;

	v[0].uv = { u0 , v0 };  // 左上のUV座標
	v[1].uv = { u1 , v0 };  // 右上のUV座標
	v[2].uv = { u0 , v1 };  // 左下のUV座標
	v[3].uv = { u1 , v1 };  // 右下のUV座標

	//a -= 0.02f; // UV座標をアニメーションさせるための変数

	// 頂点バッファのロックを解除  
	g_pContext->Unmap(g_pVertexBuffer, 0);
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定  
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// 頂点シェーダーに変換行列を設定  
	//Shader_SetMatrix(XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f));

	// プリミティブトポロジ設定  
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Texture_Set(texid); // テクスチャを設定

	// ポリゴン描画命令発行  
	g_pContext->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(int texid, float sx, float sy, float sw, float sh, int pixx, int pixy, int pixw, int pixh,
	const DirectX::XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定  
	Shader_Begin();

	// 頂点バッファをロックする  
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得  
	Vertex* v = (Vertex*)msr.pData;

	// 画面の左上から右下に向かう線分を描画する  
	v[0].position = { sx       ,sy , 0.0f };
	v[1].position = { sx + sw,sy , 0.0f };
	v[2].position = { sx       ,sy + sh, 0.0f };
	v[3].position = { sx + sw,sy + sh, 0.0f };

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	float tw = (float)Texture_GetWidth(texid);
	float th = (float)Texture_GetHeight(texid);


	float u0 = pixx / tw;
	float v0 = pixy / th;
	float u1 = (pixx + pixw) / tw;
	float v1 = (pixy + pixh) / th;

	v[0].uv = { u0 , v0 };  // 左上のUV座標
	v[1].uv = { u1 , v0 };  // 右上のUV座標
	v[2].uv = { u0 , v1 };  // 左下のUV座標
	v[3].uv = { u1 , v1 };  // 右下のUV座標

	//a -= 0.02f; // UV座標をアニメーションさせるための変数

	// 頂点バッファのロックを解除  
	g_pContext->Unmap(g_pVertexBuffer, 0);
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定  
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// 頂点シェーダーに変換行列を設定  
	//Shader_SetMatrix(XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f));

	// プリミティブトポロジ設定  
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Texture_Set(texid); // テクスチャを設定

	// ポリゴン描画命令発行  
	g_pContext->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(int texid, float sx, float sy, float sw, float sh, int pixx, int pixy, int pixw, int pixh, float angle, const DirectX::XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定  
	Shader_Begin();

	// 頂点バッファをロックする  
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得  
	Vertex* v = (Vertex*)msr.pData;

	// 画面の左上から右下に向かう線分を描画する  
	//v[0].position = { sx       ,sy , 0.0f };
	//v[1].position = { sx + sw,sy , 0.0f };
	//v[2].position = { sx       ,sy + sh, 0.0f };
	//v[3].position = { sx + sw,sy + sh, 0.0f };

	v[0].position = {-0.5f,-0.5f,0.0f};
	v[1].position = { +0.5f,-0.5f,0.0f };
	v[2].position = { -0.5f,+0.5f,0.0f };
	v[3].position = { +0.5f,+0.5f,0.0f };

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	float tw = (float)Texture_GetWidth(texid);
	float th = (float)Texture_GetHeight(texid);


	float u0 = pixx / tw;
	float v0 = pixy / th;
	float u1 = (pixx + pixw) / tw;
	float v1 = (pixy + pixh) / th;

	v[0].uv = { u0 , v0 };  // 左上のUV座標
	v[1].uv = { u1 , v0 };  // 右上のUV座標
	v[2].uv = { u0 , v1 };  // 左下のUV座標
	v[3].uv = { u1 , v1 };  // 右下のUV座標

	//a -= 0.02f; // UV座標をアニメーションさせるための変数

	// 頂点バッファのロックを解除  
	g_pContext->Unmap(g_pVertexBuffer, 0);
	XMMATRIX rotation = XMMatrixRotationZ(angle); // 回転行列を作成
	XMMATRIX translation = XMMatrixTranslation(sx + sw / 2.0f, sy + sh / 2.0f, 0.0f);
	XMMATRIX scale = XMMatrixScaling(sw, sh, 1.0f);
	Shader_SetWorldMatrix(scale * rotation * translation);

	// 頂点バッファを描画パイプラインに設定  
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// 頂点シェーダーに変換行列を設定  
	//Shader_SetMatrix(XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f));

	// プリミティブトポロジ設定  
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Texture_Set(texid); // テクスチャを設定

	// ポリゴン描画命令発行  
	g_pContext->Draw(NUM_VERTEX, 0);
}


void Sprite_Draw(int texid,float sx,float sy,float sw,float sh,
	const DirectX::XMFLOAT4& color)
{  
	// シェーダーを描画パイプラインに設定  
	Shader_Begin();

	// 頂点バッファをロックする  
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得  
	Vertex* v = (Vertex*)msr.pData;

	unsigned int dw = Texture_GetWidth(texid);
	unsigned int dh = Texture_GetHeight(texid);


	// 画面の左上から右下に向かう線分を描画する  
	v[0].position = { sx     ,sy , 0.0f };
	v[1].position = { sx + sw,sy , 0.0f };
	v[2].position = { sx     ,sy + sh, 0.0f };
	v[3].position = { sx + sw,sy + sh, 0.0f };

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	v[0].uv = { 0    , 0 };  // 左上のUV座標
	v[1].uv = { 1.0f , 0.0f };  // 右上のUV座標
	v[2].uv = { 0.0f , 1.0f };  // 左下のUV座標
	v[3].uv = { 1.0f , 1.0f };  // 右下のUV座標

	// 頂点バッファのロックを解除  
	g_pContext->Unmap(g_pVertexBuffer, 0);
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定  
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// 頂点シェーダーに変換行列を設定  
	//Shader_SetMatrix(XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f));

	// プリミティブトポロジ設定  
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Texture_Set(texid); // テクスチャを設定

	// ポリゴン描画命令発行  
	g_pContext->Draw(NUM_VERTEX, 0);

}
