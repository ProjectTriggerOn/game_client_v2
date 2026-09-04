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
// protect against a handle outliving the voice it names: the only loop today is
// the ambient bed, whose handle is held across a scene change and whose slot can
// be recycled by StopLoop or by a failed restart, so a stale Audio_StopLoop /
// Audio_SetLoopPosition must be a no-op rather than a write into whatever now
// occupies that slot.
//-----------------------------------------------------------------------------
struct AudioHandle {
    uint32_t bits = 0;
    bool IsValid() const { return bits != 0; }
};

void Audio_Initialize();
void Audio_Finalize();

// Frame boundary.  Reclaims the voices whose one-shots finished, freeing their
// slots and their share of the global voice budget.  Call FIRST in the frame,
// before any Audio_Play* — reclaiming after the frame's plays would charge this
// frame's budget for last frame's finished sounds and drop a live one.
void Audio_BeginFrame();

// Move the ears.  Call after the scene update, so this frame's spatialisation
// uses this frame's camera rather than the previous frame's.
void Audio_SetListener(const AudioListener& listener);

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

// Debug readout: how many voices are currently allocated across all pools.
int Audio_ActiveVoiceCount();
