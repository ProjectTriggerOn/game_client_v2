//=============================================================================
// test_audio_catalog.cpp — standalone unit test for audio_catalog.h.
// NOT in the vcxproj.  Follows the Game/tests/ standalone convention:
//   cl /nologo /std:c++17 /EHsc /W4 /I . /I Audio ^
//      Game\tests\test_audio_catalog.cpp Audio\audio_catalog.cpp ^
//      /Fe:_test_audio_catalog.exe && _test_audio_catalog.exe
// Run from the repo root inside a vcvars64 shell.
//=============================================================================
#include "../../Audio/audio_catalog.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } } while (0)

int main()
{
    CHECK(AudioCatalog_Load("config/audio_catalog.toml"), "catalog should load");

    // Every SoundId must resolve to a table in the TOML.
    for (uint16_t i = 0; i < (uint16_t)SoundId::Count; ++i) {
        const SoundDef& d = AudioCatalog_Get((SoundId)i);
        if (!d.IsValid()) {
            std::printf("FAIL: SoundId %u (%s) has no files\n", i, AudioCatalog_KeyOf((SoundId)i));
            ++g_fail;
        }
    }

    // Weapon fire: five variants, spatial off by default (the local viewmodel
    // plays it 2D; the remote-player path passes a position explicitly).
    const SoundDef& fire = AudioCatalog_Get(SoundId::WeaponFire);
    CHECK(fire.files.size() == 5,      "WeaponFire should have 5 variants");
    CHECK(fire.bus == AudioBus::Sfx,   "WeaponFire should sit on the Sfx bus");
    CHECK(fire.maxInstances >= 8,      "WeaponFire needs headroom for concurrent shooters");
    CHECK(fire.pitchCents > 0.0f,      "WeaponFire should randomise pitch");

    // Ambient loop is the one entry that loops and is not spatialised.
    const SoundDef& amb = AudioCatalog_Get(SoundId::AmbientLoop);
    CHECK(amb.loop,                        "AmbientLoop should loop");
    CHECK(amb.bus == AudioBus::Ambient,    "AmbientLoop should sit on the Ambient bus");

    // A missing file must degrade that one entry only, never throw or abort.
    CHECK(!AudioCatalog_Load("config/does_not_exist.toml"), "missing catalog returns false");

    std::printf(g_fail ? "\n%d FAILED\n" : "\nALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
