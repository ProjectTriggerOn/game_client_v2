//=============================================================================
// audio_events.cpp
//=============================================================================
#include "audio_events.h"
#include <cmath>

namespace {

bool Rise(uint32_t prevFlags, uint32_t currFlags, uint32_t bit)
{
    return (currFlags & bit) != 0 && (prevFlags & bit) == 0;
}

float HorizontalSpeed(const DirectX::XMFLOAT3& v)
{
    return std::sqrt(v.x * v.x + v.z * v.z);
}

// uint16 subtraction wraps naturally, so a counter rolling over 65535 reads as
// the small delta it really is rather than as a 65000-shot burst.
uint16_t CounterDelta(uint16_t prev, uint16_t curr)
{
    return (uint16_t)(curr - prev);
}

} // namespace

void AudioEventState::Reset()
{
    for (uint8_t i = 0; i < MAX_PLAYERS; ++i) ResetPlayer(i);
    lastShownKillSeq = 0;
    killSeqPrimed    = false;
}

void AudioEventState::ResetPlayer(uint8_t playerId)
{
    if (playerId >= MAX_PLAYERS) return;
    prev[playerId]          = NetPlayerState{};
    valid[playerId]         = false;
    footstepPhase[playerId] = 0.0;
}

uint8_t AudioEvents_Derive(AudioEventState& st, const Snapshot& snap,
                           double elapsed_time, AudioEvent* out, uint8_t maxEvents)
{
    uint8_t n = 0;
    auto push = [&](SoundId id, uint8_t pid) {
        if (n < maxEvents) out[n++] = AudioEvent{ id, pid, 1.0f };
    };

    bool seen[MAX_PLAYERS] = {};

    // -------------------------------------------------------------------------
    // Per-player derivation.  The local player is handled by the same code with
    // one carve-out: their shots and reloads are played by the prediction path
    // the instant the input is taken, so replaying them from the snapshot one
    // RTT later would double every round.
    // -------------------------------------------------------------------------
    auto derivePlayer = [&](uint8_t pid, const NetPlayerState& cur, bool isLocal) {
        if (pid >= MAX_PLAYERS) return;
        seen[pid] = true;

        const uint8_t emitId = isLocal ? AUDIO_EVENT_LOCAL : pid;

        if (!st.valid[pid]) {           // first sight of this slot: prime only
            st.prev[pid]          = cur;
            st.valid[pid]         = true;
            st.footstepPhase[pid] = 0.0;
            return;
        }

        const NetPlayerState& pv = st.prev[pid];

        if (!isLocal) {
            const uint16_t shots = CounterDelta(pv.fireCounter, cur.fireCounter);
            if (shots > 0) {
                const uint16_t play = (shots > AudioEventConfig::MAX_CATCHUP_SHOTS)
                                    ? AudioEventConfig::MAX_CATCHUP_SHOTS : shots;
                for (uint16_t i = 0; i < play; ++i) push(SoundId::WeaponFire, emitId);
            }

            if (Rise(pv.stateFlags, cur.stateFlags, NetStateFlags::IS_RELOADING)) {
                push((cur.stateFlags & NetStateFlags::IS_RELOAD_EMPTY)
                         ? SoundId::WeaponReloadEmpty : SoundId::WeaponReload,
                     emitId);
            }
        }

        if (Rise(pv.stateFlags, cur.stateFlags, NetStateFlags::IS_JUMPING))
            push(SoundId::JumpStart, emitId);

        if (Rise(pv.stateFlags, cur.stateFlags, NetStateFlags::IS_GROUNDED))
            push(SoundId::JumpLand, emitId);

        if (cur.health < pv.health)
            push(SoundId::TakeDamage, emitId);

        if (Rise(pv.stateFlags, cur.stateFlags, NetStateFlags::IS_DEAD))
            push(SoundId::Death, emitId);

        // Footsteps: a metronome per player, paced by horizontal speed.  Phase
        // is held rather than reset so a player who stops and starts does not
        // get a step on every keypress.
        const bool grounded = (cur.stateFlags & NetStateFlags::IS_GROUNDED) != 0;
        const bool dead     = (cur.stateFlags & NetStateFlags::IS_DEAD) != 0;
        const float speed   = HorizontalSpeed(cur.velocity);

        if (grounded && !dead && speed >= AudioEventConfig::FOOTSTEP_SPEED_MIN) {
            const double interval = (speed >= AudioEventConfig::FOOTSTEP_SPEED_RUN)
                                  ? AudioEventConfig::FOOTSTEP_INTERVAL_RUN
                                  : AudioEventConfig::FOOTSTEP_INTERVAL_WALK;
            st.footstepPhase[pid] += elapsed_time;
            while (st.footstepPhase[pid] >= interval) {
                st.footstepPhase[pid] -= interval;
                push(SoundId::Footstep, emitId);
            }
        } else {
            st.footstepPhase[pid] = 0.0;
        }

        st.prev[pid] = cur;
    };

    derivePlayer(snap.localPlayerId, snap.localPlayer, true);

    for (uint8_t i = 0; i < snap.remotePlayerCount && i < MAX_PLAYERS - 1; ++i)
        derivePlayer(snap.remotePlayers[i].playerId, snap.remotePlayers[i].state, false);

    // A slot absent from this snapshot has left.  Invalidate it so a future
    // occupant primes fresh instead of diffing against the previous player's
    // counters — otherwise slot reuse fires a phantom burst.
    for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
        if (!seen[i] && st.valid[i]) st.ResetPlayer(i);

    // -------------------------------------------------------------------------
    // Kill feed.  Same level-triggered-counter problem as the HUD feed: replay
    // (lastShown, latest] out of the ring and dedup by sequence.
    // -------------------------------------------------------------------------
    if (!st.killSeqPrimed) {
        st.lastShownKillSeq = snap.latestKillSeq;
        st.killSeqPrimed    = true;
    } else if (snap.latestKillSeq > st.lastShownKillSeq) {
        uint32_t from = snap.latestKillSeq > KILL_FEED_SIZE
                      ? snap.latestKillSeq - KILL_FEED_SIZE : 0;
        if (from < st.lastShownKillSeq) from = st.lastShownKillSeq;

        for (uint32_t seq = from; seq < snap.latestKillSeq; ++seq) {
            const KillFeedEntry& e = snap.recentKills[seq % KILL_FEED_SIZE];
            if (e.killerId == snap.localPlayerId && e.victimId != snap.localPlayerId)
                push(SoundId::KillConfirm, AUDIO_EVENT_LOCAL);
        }
        st.lastShownKillSeq = snap.latestKillSeq;
    }

    return n;
}
