#pragma once
//=============================================================================
// audio_backend.h
//
// The seam between the facade and whichever audio library is underneath.
// Deliberately free of ma_* types: swapping backends means writing another
// audio_backend_*.cpp against this header and changing nothing else.
//=============================================================================
#include "audio.h"

namespace AudioBackend {

bool Initialize();     // false => the facade goes silent-degraded
void Finalize();

void SetListener(const AudioListener& listener);
void Update(double elapsed_time);      // reclaim finished voices, age the pool

// world == nullptr means play 2D.
void     PlayOneShot(SoundId id, const DirectX::XMFLOAT3* world, float gainScale);
uint32_t PlayLoop   (SoundId id, const DirectX::XMFLOAT3* world);   // 0 = failed
void     StopLoop   (uint32_t handleBits);
void     SetLoopPosition(uint32_t handleBits, const DirectX::XMFLOAT3& world);

void SetBusVolume(AudioBus bus, float linear01);

int ActiveVoiceCount();   // debug panel

} // namespace AudioBackend
