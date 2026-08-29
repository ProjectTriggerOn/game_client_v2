#pragma once
//=============================================================================
// audio_catalog.h
//
// The sound table: logical SoundId -> files + playback parameters, loaded from
// config/audio_catalog.toml.  Developer data, deliberately separate from
// config.toml (which holds the player-facing volume settings).
//=============================================================================
#include "audio.h"
#include <string>
#include <vector>

struct SoundDef {
    std::vector<std::string> files;          // variants; one is picked at random per play
    AudioBus bus          = AudioBus::Sfx;
    bool     loop         = false;
    // There is deliberately no `spatial` flag: the call site already decides.
    // Audio_PlayOneShot is 2D, Audio_PlayOneShotAt is 3D, and the backend keys
    // off whether it was handed a position.  A catalog flag would be a second,
    // silently-ignored source of truth.
    float    gainDb       = 0.0f;
    float    pitchCents   = 0.0f;            // +/- random range, 0 = no randomisation
    uint8_t  maxInstances = 4;               // concurrent voices before stealing kicks in
    uint8_t  priority     = 128;             // higher survives; ties steal the oldest
    float    minDistance  = 1.0f;            // metres; no attenuation closer than this
    float    maxDistance  = 60.0f;
    float    rolloff      = 1.0f;

    bool IsValid() const { return !files.empty(); }
};

// Returns false if the file is missing or unparseable.  Individual entries that
// fail validation are left invalid (empty files) — the rest still load, because
// a broken sound must never take the game down (spec §10).
bool AudioCatalog_Load(const char* tomlPath);

const SoundDef& AudioCatalog_Get(SoundId id);

// The TOML table name for an id — used in log lines so a failure names the
// sound rather than a number.
const char* AudioCatalog_KeyOf(SoundId id);
