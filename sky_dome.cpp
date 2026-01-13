#include "sky_dome.h"

#include "direct3d.h"
#include "model.h"
#include "shader_3d_unlit.h"
#include "DirectXMath.h"
using namespace DirectX;
namespace 
{
	MODEL* g_pSkyDomeModel = nullptr;
	XMFLOAT3 g_Position = { 0.0f, 0.0f, 0.0f };
}

bool SkyDome_Initialize()
{
	g_pSkyDomeModel = ModelLoad("resource/model/sky.fbx", 50.0f,true);

	return true;
}

void SkyDome_Finalize()
{
	ModelRelease(g_pSkyDomeModel);
}

void SkyDome_SetPosition(const DirectX::XMFLOAT3& position)
{
	g_Position = position;
}



void SkyDome_Draw()
{
	Direct3D_SetDepthEnable(false);
	Direct3D_SetCullMode(D3D11_CULL_NONE);

	Shader_3DUnlit_Begin();

	ModelDrawUnlit(g_pSkyDomeModel, XMMatrixTranslationFromVector(XMLoadFloat3(&g_Position)));

	Direct3D_SetCullMode(D3D11_CULL_BACK);
	Direct3D_SetDepthEnable(true);
}
