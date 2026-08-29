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

//-----------------------------------------------------------------------------
// AudioListener — where the ears are.  Fed from the camera each frame.
//-----------------------------------------------------------------------------
struct AudioListener {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 front;
    DirectX::XMFLOAT3 up;
};

//-----------------------------------------------------------------------------
// AudioHandle — index + generation.  Only looping sounds get one; one-shots
// need no handle because there is nothing to stop or move.  The generation bits
// make a stale handle a no-op instead of a dangling write, which matters
// because a remote player can disconnect while their footstep loop is live.
//-----------------------------------------------------------------------------
struct AudioHandle {
    uint32_t bits = 0;
    bool IsValid() const { return bits != 0; }
};

void Audio_Initialize();
void Audio_Finalize();

// Call once per frame, before anything plays, so this frame's sounds are
// positioned against this frame's listener.
void Audio_Update(double elapsed_time, const AudioListener& listener);

void        Audio_PlayOneShot  (SoundId id, float gainScale = 1.0f);
void        Audio_PlayOneShotAt(SoundId id, const DirectX::XMFLOAT3& world, float gainScale = 1.0f);
AudioHandle Audio_PlayLoop     (SoundId id, const DirectX::XMFLOAT3* world = nullptr);
void        Audio_StopLoop     (AudioHandle h);
void        Audio_SetLoopPosition(AudioHandle h, const DirectX::XMFLOAT3& world);
void        Audio_SetBusVolume (AudioBus bus, float linear01);

// False while the system is in silent-degraded mode (no device, empty catalog).
// Callers never need to check — every Audio_* call is a no-op then — but the
// debug panel reports it.
bool Audio_IsAvailable();
