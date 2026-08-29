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
    lastTickId       = 0;
    tickPrimed       = false;
    staleSeconds     = 0.0;
}

void AudioEventState::ResetPlayer(uint8_t playerId)
{
    if (playerId >= MAX_PLAYERS) return;
    prev[playerId]          = NetPlayerState{};
    valid[playerId]         = false;
    footstepPhase[playerId] = 0.0;
}

uint8_t AudioEvents_Derive(AudioEventState& st, const Snapshot& snap,
                           double elapsed_time, AudioEvent* out, uint8_t maxEvents,
                           uint8_t* outDropped)
{
    uint8_t n       = 0;
    uint8_t dropped = 0;
    auto push = [&](SoundId id, uint8_t pid) {
        if (n < maxEvents) out[n++] = AudioEvent{ id, pid, 1.0f };
        else if (dropped < 0xFF) ++dropped;
    };

    // -------------------------------------------------------------------------
    // Staleness.  The caller re-derives from the newest snapshot every frame,
    // and "a snapshot arrived at some point this session" is not the same claim
    // as "this snapshot is current".  On an ENet stall or drop the same
    // snapshot is handed in frame after frame; the delta-based signals are
    // inert then (nothing changed, so no edge and no counter delta), but the
    // footstep metronome integrates real frame time against a frozen velocity
    // and would keep a stalled runner's steps going forever.
    //
    // Freezing dt rather than returning early is deliberate: it neutralises the
    // one time-integrating signal and leaves everything else untouched, so the
    // instant the stream resumes the normal delta path picks up where it left
    // off with no re-priming and no special case.
    if (!st.tickPrimed || snap.tickId != st.lastTickId) {
        st.lastTickId   = snap.tickId;
        st.tickPrimed   = true;
        st.staleSeconds = 0.0;
    } else {
        st.staleSeconds += elapsed_time;
        if (st.staleSeconds > AudioEventConfig::SNAPSHOT_STALE_SECONDS) elapsed_time = 0.0;
    }

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
            // A reset and heavy packet loss both look like a large forward
            // delta under wrap arithmetic; only plausibility tells them apart.
            // Above the threshold, the counter was reset (player stayed in
            // the snapshot, e.g. a match restart) — re-prime by falling
            // through to st.prev[pid] = cur below and emit nothing, rather
            // than clamping it into a phantom burst of catch-up shots.
            if (shots > 0 && shots <= AudioEventConfig::FIRE_COUNTER_RESET_THRESHOLD) {
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
    } else {
        // A match/server reset zeroes latestKillSeq while this layer still
        // holds the previous match's final count; without this, the forward
        // check below never re-arms and kill-confirm stays silent for the
        // rest of the next match.  Mirrors Game/game.cpp's HUD kill-feed fix.
        if (snap.latestKillSeq < st.lastShownKillSeq) st.lastShownKillSeq = snap.latestKillSeq;

        if (snap.latestKillSeq > st.lastShownKillSeq) {
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
    }

    if (outDropped) *outDropped = dropped;
    return n;
}
