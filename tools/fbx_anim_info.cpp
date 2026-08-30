//=============================================================================
// fbx_anim_info.cpp - standalone diagnostic tool.
// NOT in the vcxproj. Follows the Game/tests/ standalone convention:
//   For each FBX path given on the command line, prints every animation the
//   file contains: index, name, duration in ticks, ticks-per-second, and the
//   derived duration in seconds. This exists so animation lengths (e.g. for
//   matching an audio cue to a weapon animation) can be re-measured on demand
//   instead of by hand, the way the reload durations in Network/net_common.h
//   were originally derived.
//
// Build (from repo root, inside a vcvars64 PowerShell, not Git Bash):
//   cl /nologo /std:c++17 /EHsc /W4 /I ThirdParty\assimp\include ^
//      tools\fbx_anim_info.cpp /Fe:fbx_anim_info.exe ^
//      /link /LIBPATH:ThirdParty\assimp\lib assimp-vc143-mt.lib
//
// Run: assimp-vc143-mt.dll (repo root) must be next to fbx_anim_info.exe, or
// otherwise reachable on PATH, at run time.
//   .\fbx_anim_info.exe resource\model\red_arm003.fbx resource\model\blue_arm003.fbx
//
// Import flags are deliberately minimal (0): only animation metadata is
// needed here, not triangulated/skinned geometry, so no post-processing
// passes are requested.
//=============================================================================
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <cstdio>

namespace
{
    void DumpFile(const char* path)
    {
        std::printf("=== %s ===\n", path);

        const aiScene* scene = aiImportFile(path, 0u);
        if (!scene)
        {
            std::printf("  ERROR: failed to import: %s\n", aiGetErrorString());
            return;
        }

        if (scene->mNumAnimations == 0)
        {
            std::printf("  (no animations found)\n");
            aiReleaseImport(scene);
            return;
        }

        for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
        {
            const aiAnimation* anim = scene->mAnimations[i];
            const char* name = anim->mName.length > 0 ? anim->mName.C_Str() : "(unnamed)";

            std::printf("  [%u] name=\"%s\" duration_ticks=%.6f ticks_per_second=%.6f duration_seconds=",
                        i, name, anim->mDuration, anim->mTicksPerSecond);

            if (anim->mTicksPerSecond > 0.0)
            {
                double seconds = anim->mDuration / anim->mTicksPerSecond;
                std::printf("%.6f\n", seconds);
            }
            else
            {
                // Some exporters emit 0 (or omit) ticks-per-second; dividing by
                // that would be a divide-by-zero, and guessing a fallback rate
                // would silently bake a wrong number into whatever consumes
                // this, so report the fact plainly instead.
                std::printf("N/A (ticks_per_second is zero/absent, cannot compute)\n");
            }
        }

        aiReleaseImport(scene);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <file.fbx> [more.fbx ...]\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; ++i)
    {
        DumpFile(argv[i]);
    }

    return 0;
}
