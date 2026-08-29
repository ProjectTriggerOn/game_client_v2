#pragma once
//=============================================================================
// audio_events.h
//
// Turns server snapshots into discrete audio events.  Pure in the sense that
// matters: no audio API, no device, no globals — every bit of carried state
// lives in AudioEventState, so the whole layer can be driven frame by frame
// from a test with no sound card.
//
// The client never receives audio event packets.  Everything below is derived
// from state the snapshot already carries, which is why this feature needs no
// server change at all.
//=============================================================================
#include "audio.h"
#include "../Network/net_common.h"

// playerId sentinel: the event belongs to the local player / the UI and is
// played 2D.  A gun in your own hands has no distance falloff.
constexpr uint8_t AUDIO_EVENT_LOCAL = 0xFF;

struct AudioEvent {
    SoundId id        = SoundId::Count;
    uint8_t playerId  = AUDIO_EVENT_LOCAL;   // resolved to a world position by the caller
    float   gainScale = 1.0f;
};

namespace AudioEventConfig {
// A lost snapshot leaves a fireCounter gap.  Replaying it shot for shot dumps
// a wall of gunfire into one frame, so cap the catch-up: the player hears that
// someone was shooting without the mix collapsing.
constexpr uint16_t MAX_CATCHUP_SHOTS = 2;

// A match/session reset zeroes every player's fireCounter while the player
// stays present in the snapshot (see mock_server.cpp's re-arm path), so the
// wrap-safe delta reads as a huge forward jump — the same shape as heavy
// packet loss.  A delta above this ceiling is implausible as a real shot
// count between two snapshots, so treat it as a reset: re-prime silently
// instead of clamping it into a burst of catch-up shots.
constexpr uint16_t FIRE_COUNTER_RESET_THRESHOLD = 256;

constexpr float  FOOTSTEP_SPEED_MIN     = 0.5f;   // m/s below this: standing still
constexpr float  FOOTSTEP_SPEED_RUN     = 5.0f;   // m/s above this: use the run cadence
constexpr double FOOTSTEP_INTERVAL_WALK = 0.45;   // seconds between steps
constexpr double FOOTSTEP_INTERVAL_RUN  = 0.30;
} // namespace AudioEventConfig

//-----------------------------------------------------------------------------
// AudioEventState — everything the derivation carries between frames.  A
// struct rather than file statics precisely so tests can construct it and
// ResetPlayer can clear one slot when a player leaves.
//-----------------------------------------------------------------------------
struct AudioEventState {
    NetPlayerState prev[MAX_PLAYERS]{};
    bool           valid[MAX_PLAYERS]{};       // false: prime this slot, emit nothing
    double         footstepPhase[MAX_PLAYERS]{};
    uint32_t       lastShownKillSeq = 0;
    bool           killSeqPrimed    = false;

    void Reset();
    void ResetPlayer(uint8_t playerId);
};

// Returns the number of events written to out[] (never more than maxEvents;
// the overflow is dropped and logged by the caller).
uint8_t AudioEvents_Derive(AudioEventState& state, const Snapshot& snap,
                           double elapsed_time, AudioEvent* out, uint8_t maxEvents);
