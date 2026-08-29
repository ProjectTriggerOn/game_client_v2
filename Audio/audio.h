#pragma once
//=============================================================================
// audio.h
//
// Game-facing audio facade.  Nothing in this header may leak the backend:
// no ma_* types, no <miniaudio.h>.  Swapping the backend means rewriting
// audio_backend_miniaudio.cpp and nothing else.
//=============================================================================
#include <DirectXMath.h>
#include <cstdint>

//-----------------------------------------------------------------------------
// SoundId — logical sounds.  Each value maps to a same-named table in
// config/audio_catalog.toml; the mapping lives in audio_catalog.cpp's
// kSoundKeys[] and is asserted to be complete at load time.
//-----------------------------------------------------------------------------
enum class SoundId : uint16_t {
    WeaponFire,
    WeaponFireEmpty,
    WeaponReload,
    WeaponReloadEmpty,
    Footstep,
    JumpStart,
    JumpLand,
    Hitmarker,
    TakeDamage,
    Death,
    KillConfirm,
    AdsIn,
    AdsOut,
    UiClick,
    AmbientLoop,
    Count
};

//-----------------------------------------------------------------------------
// AudioBus — mixing groups.  Master is the engine's own volume; the other
// four are sound groups underneath it.  Volumes come from config.toml [audio].
//-----------------------------------------------------------------------------
enum class AudioBus : uint8_t { Master, Sfx, Ui, Music, Ambient, Count };
