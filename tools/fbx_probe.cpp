//=============================================================================
// fbx_probe.cpp — report what the engine will actually make of an FBX.
//
// Every question that matters about a model asset is answered here, through the
// exact Assimp flags Graphics/model.cpp ModelLoad uses and with the node
// transform baked in the same way:
//
//   * bounds and pivot   — is it the size you think, and where is its base?
//   * units              — the Low Poly Shooter Pack mixes metres and cm
//   * orientation        — is the tall axis Y, or is the asset on its back?
//   * materials          — diffuse colour and the sidecar texture it names
//   * vertex channels    — a mesh with no UV0 or no normals
//
// That last one is not academic: aiProcess_SortByPType can hand back a sub-mesh
// with no UV set, ModelLoad used to dereference it blind, and a map referencing
// such an asset took the client down with an access violation.
//
// Build and run from the client repo root, inside a VS developer prompt:
//   cl /nologo /std:c++17 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS
//      /I ThirdParty\assimp\include tools\fbx_probe.cpp /Fe:fbx_probe.exe
//      /link /LIBPATH:ThirdParty\assimp\lib assimp-vc143-mt.lib
//   fbx_probe.exe resource\model\*.fbx
//=============================================================================
#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/material.h"

#include <cfloat>
#include <cstdio>
#include <cstring>

namespace {

struct Bounds {
    aiVector3D mn{ FLT_MAX, FLT_MAX, FLT_MAX };
    aiVector3D mx{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
};

// ModelLoad bakes each mesh's accumulated node transform into its vertices;
// walk the hierarchy the same way so the numbers below are the rendered ones.
void Accumulate(const aiScene* scene, const aiNode* node, aiMatrix4x4 acc,
                Bounds& b, unsigned& tris) {
    acc = acc * node->mTransformation;
    for (unsigned i = 0; i < node->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        tris += mesh->mNumFaces;
        for (unsigned v = 0; v < mesh->mNumVertices; v++) {
            const aiVector3D p = acc * mesh->mVertices[v];
            if (p.x < b.mn.x) b.mn.x = p.x;
            if (p.y < b.mn.y) b.mn.y = p.y;
            if (p.z < b.mn.z) b.mn.z = p.z;
            if (p.x > b.mx.x) b.mx.x = p.x;
            if (p.y > b.mx.y) b.mx.y = p.y;
            if (p.z > b.mx.z) b.mx.z = p.z;
        }
    }
    for (unsigned c = 0; c < node->mNumChildren; c++)
        Accumulate(scene, node->mChildren[c], acc, b, tris);
}

const char* BaseName(const char* path) {
    const char* a = std::strrchr(path, '/');
    const char* b = std::strrchr(path, '\\');
    const char* p = a > b ? a : b;
    return p ? p + 1 : path;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: fbx_probe <file.fbx> [more.fbx ...]\n");
        return 2;
    }

    int problems = 0;
    for (int i = 1; i < argc; i++) {
        const aiScene* scene = aiImportFile(argv[i],
            static_cast<unsigned>(aiProcessPreset_TargetRealtime_MaxQuality
                                | aiProcess_ConvertToLeftHanded
                                | aiProcess_GenBoundingBoxes));
        const char* name = BaseName(argv[i]);
        if (!scene) {
            std::printf("%-30s IMPORT FAILED\n", name);
            problems++;
            continue;
        }

        Bounds b;
        unsigned tris = 0;
        Accumulate(scene, scene->mRootNode, aiMatrix4x4(), b, tris);

        const float sx = b.mx.x - b.mn.x;
        const float sy = b.mx.y - b.mn.y;
        const float sz = b.mx.z - b.mn.z;
        const float biggest = sx > sy ? (sx > sz ? sx : sz) : (sy > sz ? sy : sz);
        // The pack mixes authoring units; anything over 10 units across is
        // centimetres. tools/shipment_convert.cpp applies the same rule.
        const float unit = biggest > 10.0f ? 0.01f : 1.0f;

        std::printf("%-30s %6.2f x %6.2f x %6.2f m   base %+6.2f   unit %.2f   "
                    "tris %5u   meshes %u\n",
                    name, sx * unit, sy * unit, sz * unit, b.mn.y * unit, unit,
                    tris, scene->mNumMeshes);

        for (unsigned m = 0; m < scene->mNumMeshes; m++) {
            const aiMesh* mesh = scene->mMeshes[m];
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            aiColor3D diffuse(0, 0, 0);
            aiString tex, matName;
            mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
            mat->Get(AI_MATKEY_NAME, matName);
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &tex);

            const bool hasUV = mesh->mTextureCoords[0] != nullptr;
            const bool hasNormals = mesh->mNormals != nullptr;
            std::printf("      mesh %u  mat '%s'  diffuse (%.2f %.2f %.2f)  "
                        "tex '%s'  uv0 %s  normals %s\n",
                        m, matName.C_Str(), diffuse.r, diffuse.g, diffuse.b,
                        tex.C_Str(), hasUV ? "yes" : "NO", hasNormals ? "yes" : "NO");
            if (!hasUV || !hasNormals) problems++;
        }
        aiReleaseImport(scene);
    }

    if (problems)
        std::printf("\n%d problem(s): a mesh without UV0 or normals will render "
                    "untextured; ModelLoad substitutes defaults rather than "
                    "dereferencing the missing channel.\n", problems);
    return problems ? 1 : 0;
}
