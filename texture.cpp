#include "texture.h"
#include "direct3d.h"
#include <string>
#include "WICTextureLoader11.h"
using namespace DirectX;
static constexpr int MAX_TEXTURES = 1024; // 最大テクスチャ数
struct Texture
{
	std::wstring filename; // ファイル名
	
	unsigned int width ; // 幅
	unsigned int height ; // 高さ

	ID3D11Resource* pTexture = nullptr;
	ID3D11ShaderResourceView* pTextureView = nullptr;
};

namespace{
	Texture g_Textures[MAX_TEXTURES]{}; // テクスチャデータ配列
	unsigned int g_SetTextureIndex = static_cast<unsigned int>(-1); // 現在設定されているテクスチャのインデックス
	
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
}

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	for (Texture& t : g_Textures)
	{
		t.pTexture = nullptr; // 初期化
		//t.width = 0;
		//t.height = 0;
	}
	g_SetTextureIndex = static_cast<unsigned int>(-1);
	g_pDevice = pDevice; // デバイスの保存
	g_pContext = pContext; // デバイスコンテキストの保存
}

void Texture_Finalize(void)
{
	Texture_AllRelease(); // 全テクスチャの解放
}

int Texture_LoadFromFile(const wchar_t* pFilename)
{

	//既に読み込んだファイルはスキップ
	for (int i = 0; i < MAX_TEXTURES; ++i) {
		if (g_Textures[i].filename == pFilename) {
			return i; // 既に読み込まれている場合はそのインデックスを返す
		}
	}

	//空いている管理領域を探す
	for (int i = 0; i < MAX_TEXTURES; ++i) {

		if (g_Textures[i].pTexture)continue;// すでに使用中のテクスチャはスキップ

		HRESULT hr;
		hr = CreateWICTextureFromFile(g_pDevice, g_pContext, pFilename, &g_Textures[i].pTexture, &g_Textures[i].pTextureView);

		ID3D11Texture2D* pTexture = (ID3D11Texture2D*)g_Textures[i].pTexture;
		D3D11_TEXTURE2D_DESC t2desc;
		pTexture->GetDesc(&t2desc);
		g_Textures[i].width = t2desc.Width;
		g_Textures[i].height = t2desc.Height;

		if (FAILED(hr)) {
			MessageBoxW(nullptr, L"テクスチャの読み込みに失敗しました", pFilename, MB_OK | MB_ICONERROR);
			return -1; // 読み込み失敗
		}

		g_Textures[i].filename = pFilename; // ファイル名を保存

		return i;
		
	}

	return -1; // 空いている領域がない場合は-1を返す
}

void Texture_AllRelease()
{
	for (Texture& t:g_Textures)
	{
		t.filename.clear(); // ファイル名をクリア
		SAFE_RELEASE(t.pTexture)// テクスチャの解放)
		SAFE_RELEASE(t.pTextureView)
	}
}

void Texture_Release(int texid)
{
	if (texid < 0 || texid >= MAX_TEXTURES)return; // 範囲外のインデックスは無視
	SAFE_RELEASE(g_Textures[texid].pTexture)
	SAFE_RELEASE(g_Textures[texid].pTextureView);// テクスチャの解放
	g_Textures[texid].filename.clear(); // ファイル名をクリア
	g_Textures[texid].width = 0; // 幅をリセット
	g_Textures[texid].height = 0; // 高さをリセット
	if (g_SetTextureIndex == static_cast<unsigned int>(texid)) {
		g_SetTextureIndex = static_cast<unsigned int>(-1); // 現在のテクスチャインデックスをリセット
	}
}

void Texture_Set(int texid)
{
	if (texid < 0 || texid >= MAX_TEXTURES)return; // 範囲外のインデックスは無視
	g_SetTextureIndex = texid; // 現在のテクスチャインデックスを更新
	g_pContext->PSSetShaderResources(0, 1, &g_Textures[texid].pTextureView);

}

unsigned int Texture_GetWidth(int texid)
{
	if (texid < 0)return 0;

	return g_Textures[texid].width; // 幅を返す
}

unsigned int Texture_GetHeight(int texid)
{
	if (texid < 0)return 0;

	return g_Textures[texid].height; // 高さを返す
}
