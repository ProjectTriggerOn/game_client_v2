#include "reticle.h"

#include "config.h"

using namespace DirectX;

namespace {

// weapon.reticle_scale -- 1.0 leaves the dot at its authored size.
float g_Scale = 1.0f;

} // namespace

void Reticle_Initialize()
{
	// Config subscriptions (docs 9.2: each module owns its keys). The callers
	// run this per player and per scene entry, but subscription must happen
	// exactly once -- Subscribe also fires immediately if the key has a value,
	// so the persisted setting is applied right here.
	static bool s_subscribed = false;
	if (!s_subscribed) {
		s_subscribed = true;
		Config::GetInstance().Subscribe("weapon.reticle_scale",
		                                [](const ConfigValue& v) { g_Scale = (float)v.AsFloat(); });
	}
}

XMFLOAT3 Reticle_GetCenter(MODEL* model)
{
	XMFLOAT3 center{ 0.0f, 0.0f, 0.0f };
	if (!model || !model->AiScene) return center;

	double sx = 0.0, sy = 0.0, sz = 0.0;
	unsigned int count = 0;

	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		const aiMesh* mesh = model->AiScene->mMeshes[m];
		for (unsigned int v = 0; v < mesh->mNumVertices; v++)
		{
			sx += mesh->mVertices[v].x;
			sy += mesh->mVertices[v].y;
			sz += mesh->mVertices[v].z;
			count++;
		}
	}

	if (count == 0) return center;

	center.x = (float)(sx / count);
	center.y = (float)(sy / count);
	center.z = (float)(sz / count);
	return center;
}

void Reticle_Draw(MODEL* model, const XMFLOAT3& center, const XMMATRIX& world)
{
	if (!model) return;

	// Scale about the quad's own centre so it grows in place instead of sliding
	// along the sight. Row-vector order: translate to origin, scale, translate back.
	const XMMATRIX scale =
		XMMatrixTranslation(-center.x, -center.y, -center.z) *
		XMMatrixScaling(g_Scale, g_Scale, g_Scale) *
		XMMatrixTranslation(center.x, center.y, center.z);

	ModelDrawUnlit(model, scale * world);
}
