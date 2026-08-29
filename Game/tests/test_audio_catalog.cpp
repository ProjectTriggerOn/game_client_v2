//=============================================================================
// test_audio_catalog.cpp — standalone unit test for audio_catalog.h.
// NOT in the vcxproj.  Follows the Game/tests/ standalone convention:
//   cl /nologo /std:c++17 /EHsc /W4 /I . /I Audio /I Core ^
//      Game\tests\test_audio_catalog.cpp Audio\audio_catalog.cpp Core\debug_log.cpp ^
//      /Fe:_test_audio_catalog.exe && _test_audio_catalog.exe
// Run from the repo root inside a vcvars64 shell.
// audio_catalog.cpp logs its diagnostics through DebugLog_Printf (Core/debug_log.h),
// which is why Core\debug_log.cpp and /I Core are needed here now; debug_log.cpp
// itself only pulls in header-only project code (config.h, exe_path.h), so this
// is the one extra .cpp this test needs, not a wider cascade.
//=============================================================================
#include "../../Audio/audio_catalog.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } } while (0)

int main()
{
    CHECK(AudioCatalog_Load("config/audio_catalog.toml"), "catalog should load");

    // Pinned so that dropping an id — as `hitmarker` was, for want of any hit
    // feedback to trigger it — or adding one is a deliberate edit here and not
    // a silently narrower sweep below.
    static_assert((uint16_t)SoundId::Count == 14,
                  "SoundId count changed: update config/audio_catalog.toml and this test");

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

    // Ambient loop is the one entry the backend streams rather than decoding
    // (a ~9 MB bed), and the one entry on the Ambient bus.
    const SoundDef& amb = AudioCatalog_Get(SoundId::AmbientLoop);
    CHECK(amb.stream,                      "AmbientLoop should stream, not pre-decode");
    CHECK(amb.bus == AudioBus::Ambient,    "AmbientLoop should sit on the Ambient bus");

    // Nothing else streams: a one-shot must start from a resident buffer, and
    // the init-time preload deliberately skips whatever is marked stream.
    for (uint16_t i = 0; i < (uint16_t)SoundId::Count; ++i) {
        if ((SoundId)i == SoundId::AmbientLoop) continue;
        if (AudioCatalog_Get((SoundId)i).stream) {
            std::printf("FAIL: SoundId %u (%s) streams; only the ambient bed should\n",
                        i, AudioCatalog_KeyOf((SoundId)i));
            ++g_fail;
        }
    }

    // A missing file must degrade that one entry only, never throw or abort.
    CHECK(!AudioCatalog_Load("config/does_not_exist.toml"), "missing catalog returns false");

    // A failed reload must not destroy the previously-good catalog: the real
    // catalog loaded above has to still be intact after that failed Load call.
    CHECK(AudioCatalog_Get(SoundId::WeaponFire).files.size() == 5,
          "a failed reload must not wipe the previously-loaded catalog");

    // The headline degrade guarantee this task exists to add: a single missing
    // WAV silences only that one catalog entry, never the whole load, and it
    // must never assert/throw/abort. The real catalog has no missing files
    // (that's the point of it), so exercise this with a throwaway fixture
    // TOML instead of touching config/audio_catalog.toml.
    {
        const std::filesystem::path fixturePath =
            std::filesystem::temp_directory_path() / "trigeron_audio_catalog_test_fixture.toml";
        {
            std::ofstream f(fixturePath);
            f << "[weapon_fire]\n"
                 "files = [\"resource/audio/__test_fixture_missing__.wav\"]\n"
                 "\n"
                 "[weapon_reload]\n"
                 "files = [\"resource/audio/weapons/reload.wav\"]\n";
        }

        CHECK(AudioCatalog_Load(fixturePath.string().c_str()),
              "a catalog with one bad entry and one good entry should still load overall");

        const SoundDef& bad  = AudioCatalog_Get(SoundId::WeaponFire);
        const SoundDef& good = AudioCatalog_Get(SoundId::WeaponReload);
        CHECK(!bad.IsValid(), "an entry whose only file is missing must come back invalid, not throw/abort");
        CHECK(good.IsValid(), "an entry with a real file must still load even when a sibling entry is broken");

        std::filesystem::remove(fixturePath);

        // Leave the real catalog loaded, in case anything below ever depends on it.
        CHECK(AudioCatalog_Load("config/audio_catalog.toml"), "reloading the real catalog after the fixture test should succeed");
    }

    std::printf(g_fail ? "\n%d FAILED\n" : "\nALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
