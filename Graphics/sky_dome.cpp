#include "sky_dome.h"

#include "direct3d.h"
#include "model.h"
#include "shader_3d_unlit.h"
#include "debug_ostream.h"
#include "DirectXMath.h"
using namespace DirectX;
namespace
{
	MODEL* g_pSkyDomeModel = nullptr;
	XMFLOAT3 g_Position = { 0.0f, 0.0f, 0.0f };

	// Historical default when the map does not author a sky asset. Matches
	// ResourceFileLayout in the README: model lives under resource/model/.
	const char* kDefaultSkyAsset = "resource/model/sky.fbx";
}

bool SkyDome_Initialize(const char* assetPath)
{
	const char* path = (assetPath && assetPath[0] != '\0') ? assetPath : kDefaultSkyAsset;
	g_pSkyDomeModel = ModelLoad(path, 50.0f, true);
	if (!g_pSkyDomeModel) {
		hal::dout << "SkyDome_Initialize() : failed to load sky asset " << path << std::endl;
		return false;
	}
	return true;
}

void SkyDome_Finalize()
{
	ModelRelease(g_pSkyDomeModel);
	g_pSkyDomeModel = nullptr;
}

void SkyDome_SetPosition(const DirectX::XMFLOAT3& position)
{
	g_Position = position;
}



void SkyDome_Draw()
{
	if (!g_pSkyDomeModel) return;

	Direct3D_SetDepthEnable(false);
	Direct3D_SetCullMode(D3D11_CULL_NONE);

	Shader_3DUnlit_Begin();

	ModelDrawUnlit(g_pSkyDomeModel, XMMatrixTranslationFromVector(XMLoadFloat3(&g_Position)));

	Direct3D_SetCullMode(D3D11_CULL_BACK);
	Direct3D_SetDepthEnable(true);
}
