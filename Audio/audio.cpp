//=============================================================================
// audio.cpp — facade.  Guards every entry point with the availability flag so
// a machine with no audio device runs the game exactly as before, just silent.
//=============================================================================
#include "audio.h"
#include "audio_backend.h"
#include "audio_catalog.h"
#include "debug_log.h"
#include "config.h"

namespace {
bool g_Available = false;
}

void Audio_Initialize()
{
    if (!AudioCatalog_Load("config/audio_catalog.toml")) {
        DebugLog_Printf("audio", "catalog unavailable - running silent");
        return;
    }
    if (!AudioBackend::Initialize()) {
        DebugLog_Printf("audio", "backend init failed - running silent");
        return;
    }
    g_Available = true;

    // Subscribe fires immediately with the persisted value, so this both wires
    // up live updates and applies the saved volumes — no separate initial read.
    // Same pattern as input.sensitivity (Game/player_cam_fps.cpp) and
    // display.vsync (main.cpp).
    Config& cfg = Config::GetInstance();
    cfg.Subscribe("audio.master",  [](const ConfigValue& v) { Audio_SetBusVolume(AudioBus::Master,  (float)v.AsFloat()); });
    cfg.Subscribe("audio.sfx",     [](const ConfigValue& v) { Audio_SetBusVolume(AudioBus::Sfx,     (float)v.AsFloat()); });
    cfg.Subscribe("audio.ui",      [](const ConfigValue& v) { Audio_SetBusVolume(AudioBus::Ui,      (float)v.AsFloat()); });
    cfg.Subscribe("audio.music",   [](const ConfigValue& v) { Audio_SetBusVolume(AudioBus::Music,   (float)v.AsFloat()); });
    cfg.Subscribe("audio.ambient", [](const ConfigValue& v) { Audio_SetBusVolume(AudioBus::Ambient, (float)v.AsFloat()); });

    DebugLog_Printf("audio", "initialized");
}

void Audio_Finalize()
{
    if (!g_Available) return;
    AudioBackend::Finalize();
    g_Available = false;
}

void Audio_Update(double elapsed_time, const AudioListener& listener)
{
    if (!g_Available) return;
    AudioBackend::SetListener(listener);
    AudioBackend::Update(elapsed_time);
}

void Audio_PlayOneShot(SoundId id, float gainScale)
{
    if (!g_Available) return;
    AudioBackend::PlayOneShot(id, nullptr, gainScale);
}

void Audio_PlayOneShotAt(SoundId id, const DirectX::XMFLOAT3& world, float gainScale)
{
    if (!g_Available) return;
    AudioBackend::PlayOneShot(id, &world, gainScale);
}

AudioHandle Audio_PlayLoop(SoundId id, const DirectX::XMFLOAT3* world)
{
    AudioHandle h{};
    if (!g_Available) return h;
    h.bits = AudioBackend::PlayLoop(id, world);
    return h;
}

void Audio_StopLoop(AudioHandle h)
{
    if (!g_Available || !h.IsValid()) return;
    AudioBackend::StopLoop(h.bits);
}

void Audio_SetLoopPosition(AudioHandle h, const DirectX::XMFLOAT3& world)
{
    if (!g_Available || !h.IsValid()) return;
    AudioBackend::SetLoopPosition(h.bits, world);
}

void Audio_SetBusVolume(AudioBus bus, float linear01)
{
    if (!g_Available) return;
    AudioBackend::SetBusVolume(bus, linear01);
}

bool Audio_IsAvailable() { return g_Available; }
