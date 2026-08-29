//=============================================================================
// test_audio_events.cpp — standalone unit test for audio_events.h.
// NOT in the vcxproj.  Follows the Game/tests/ standalone convention:
//   cl /nologo /std:c++17 /EHsc /W4 /I . /I Audio ^
//      Game\tests\test_audio_events.cpp Audio\audio_events.cpp ^
//      /Fe:_test_audio_events.exe && _test_audio_events.exe
// Run from the repo root inside a vcvars64 shell.  Needs no audio device:
// that is the entire reason the derivation layer is kept free of audio calls.
//=============================================================================
#include "../../Audio/audio_events.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } } while (0)

namespace {

constexpr uint8_t LOCAL_ID = 0;
constexpr uint8_t BOT_ID   = 3;

// A snapshot with one local player and one remote (BOT_ID), both alive and
// standing still on the ground.
Snapshot MakeSnapshot()
{
    Snapshot s{};
    s.localPlayerId = LOCAL_ID;
    s.localPlayer.health     = 100;
    s.localPlayer.stateFlags = NetStateFlags::IS_GROUNDED;
    s.localPlayer.hitByPlayerId = 0xFF;

    s.remotePlayerCount = 1;
    s.remotePlayers[0].playerId = BOT_ID;
    s.remotePlayers[0].state.health     = 100;
    s.remotePlayers[0].state.stateFlags = NetStateFlags::IS_GROUNDED;
    s.remotePlayers[0].state.hitByPlayerId = 0xFF;
    return s;
}

NetPlayerState& Bot(Snapshot& s) { return s.remotePlayers[0].state; }

int CountOf(const AudioEvent* ev, uint8_t n, SoundId id)
{
    int c = 0;
    for (uint8_t i = 0; i < n; ++i) if (ev[i].id == id) ++c;
    return c;
}

// Feed one FRESH snapshot and return the events it produced.  The tick bump
// matters: the derivation treats an unchanged tickId as "no new snapshot
// arrived" and stops advancing time once the gap passes
// SNAPSHOT_STALE_SECONDS, so a test that wants a live stream has to look like
// one.  The stall test below calls AudioEvents_Derive directly to get the
// frozen-snapshot behaviour on purpose.
uint8_t Step(AudioEventState& st, Snapshot& s, double dt, AudioEvent* out)
{
    ++s.tickId;
    return AudioEvents_Derive(st, s, dt, out, 32);
}

} // namespace

int main()
{
    AudioEvent ev[32];

    // --- the first snapshot must be pure priming: no edges without a previous
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Bot(s).fireCounter = 7;
        Bot(s).stateFlags |= NetStateFlags::IS_RELOADING;
        const uint8_t n = Step(st, s, 0.016, ev);
        CHECK(n == 0, "first snapshot must not emit anything (no previous state)");
    }

    // --- fireCounter +1 -> exactly one shot
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);
        Bot(s).fireCounter = 1;
        const uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponFire) == 1, "one shot per fireCounter increment");
        CHECK(ev[0].playerId == BOT_ID, "remote shot must carry the shooter id");
    }

    // --- packet loss: a big jump is clamped, not replayed shot for shot
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);
        Bot(s).fireCounter = 40;
        const uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponFire) == AudioEventConfig::MAX_CATCHUP_SHOTS,
              "a fireCounter jump must clamp to MAX_CATCHUP_SHOTS");
    }

    // --- uint16 wrap must not read as a 65000-shot burst
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Bot(s).fireCounter = 65535;
        Step(st, s, 0.016, ev);
        Bot(s).fireCounter = 1;          // wrapped: two shots fired
        const uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponFire) == 2, "wrap-around is a small delta, not a burst");
    }

    // --- reload: rising edge only, and the empty variant is chosen correctly
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        Bot(s).stateFlags |= NetStateFlags::IS_RELOADING;
        uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponReload) == 1, "reload fires once on the rising edge");

        n = Step(st, s, 0.016, ev);      // still reloading
        CHECK(CountOf(ev, n, SoundId::WeaponReload) == 0, "held reload must not retrigger");

        Bot(s).stateFlags &= ~NetStateFlags::IS_RELOADING;
        Step(st, s, 0.016, ev);
        Bot(s).stateFlags |= NetStateFlags::IS_RELOADING | NetStateFlags::IS_RELOAD_EMPTY;
        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponReloadEmpty) == 1, "IS_RELOAD_EMPTY selects the empty reload");
        CHECK(CountOf(ev, n, SoundId::WeaponReload) == 0,      "and not the normal one");
    }

    // --- jump and land
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        Bot(s).stateFlags |=  NetStateFlags::IS_JUMPING;
        Bot(s).stateFlags &= ~NetStateFlags::IS_GROUNDED;
        uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::JumpStart) == 1, "jump fires on the rising edge");

        Bot(s).stateFlags &= ~NetStateFlags::IS_JUMPING;
        Bot(s).stateFlags |=  NetStateFlags::IS_GROUNDED;
        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::JumpLand) == 1, "land fires when grounded goes true again");
    }

    // --- damage and death
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        s.localPlayer.health = 70;
        uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::TakeDamage) == 1, "local health drop plays take-damage");
        CHECK(ev[0].playerId == AUDIO_EVENT_LOCAL,      "local damage is not spatialised");

        s.localPlayer.health = 100;      // respawn heal
        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::TakeDamage) == 0, "healing must not play take-damage");

        Bot(s).stateFlags |= NetStateFlags::IS_DEAD;
        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::Death) == 1, "death fires on the rising edge");
    }

    // --- the local player's own shots and reloads come from prediction, never
    //     from the snapshot, or they would double up
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        s.localPlayer.fireCounter = 1;
        s.localPlayer.stateFlags |= NetStateFlags::IS_RELOADING;
        const uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponFire) == 0,   "local shots must not come from the snapshot");
        CHECK(CountOf(ev, n, SoundId::WeaponReload) == 0, "local reloads must not come from the snapshot");
    }

    // --- a slot that goes away and comes back must not fire stale edges
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Bot(s).fireCounter = 50;
        Step(st, s, 0.016, ev);

        s.remotePlayerCount = 0;                  // bot disconnects
        Step(st, s, 0.016, ev);

        s = MakeSnapshot();                       // a different player reuses the slot
        Bot(s).fireCounter = 0;
        const uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponFire) == 0,
              "a reused slot must re-prime, not diff against the previous occupant");
    }

    // --- footsteps: paced by speed, silent in the air
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        // 8.0 m/s is the server's MAX_RUN_SPEED (Network/mock_server.cpp).
        Bot(s).velocity = DirectX::XMFLOAT3(8.0f, 0.0f, 0.0f);   // running
        int steps = 0;
        for (int i = 0; i < 100; ++i) steps += CountOf(ev, Step(st, s, 0.016, ev), SoundId::Footstep);
        // 100 * 16ms = 1.6s at the run interval
        const int expected = (int)(1.6 / AudioEventConfig::FOOTSTEP_INTERVAL_RUN);
        CHECK(steps >= expected - 1 && steps <= expected + 1, "run footsteps are paced by the run interval");

        // 5.0 m/s is the server's MAX_WALK_SPEED.  The walk cadence has to be
        // REACHABLE at exactly that speed: the run threshold used to sit on
        // 5.0, so a plainly walking player got the run cadence and the walk
        // interval could never be selected at all.
        Bot(s).velocity = DirectX::XMFLOAT3(5.0f, 0.0f, 0.0f);   // walking
        int walkSteps = 0;
        for (int i = 0; i < 100; ++i) walkSteps += CountOf(ev, Step(st, s, 0.016, ev), SoundId::Footstep);
        const int expectedWalk = (int)(1.6 / AudioEventConfig::FOOTSTEP_INTERVAL_WALK);
        CHECK(walkSteps >= expectedWalk - 1 && walkSteps <= expectedWalk + 1,
              "walking at the server's walk speed uses the walk cadence");
        CHECK(AudioEventConfig::FOOTSTEP_SPEED_RUN > 5.0f && AudioEventConfig::FOOTSTEP_SPEED_RUN < 8.0f,
              "the run threshold must sit strictly between the server's walk and run speeds");

        Bot(s).stateFlags &= ~NetStateFlags::IS_GROUNDED;
        int airborne = 0;
        for (int i = 0; i < 60; ++i) airborne += CountOf(ev, Step(st, s, 0.016, ev), SoundId::Footstep);
        CHECK(airborne == 0, "no footsteps while airborne");

        Bot(s).stateFlags |= NetStateFlags::IS_GROUNDED;
        Bot(s).velocity = DirectX::XMFLOAT3(0.1f, 0.0f, 0.0f);   // below the threshold
        int idle = 0;
        for (int i = 0; i < 60; ++i) idle += CountOf(ev, Step(st, s, 0.016, ev), SoundId::Footstep);
        CHECK(idle == 0, "no footsteps below the minimum speed");
    }

    // --- kill feed drives the kill-confirm sting, once per new kill
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        s.latestKillSeq = 1;
        s.recentKills[0] = KillFeedEntry{ LOCAL_ID, BOT_ID, 0, 1 };
        uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::KillConfirm) == 1, "a new kill by the local player stings once");

        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::KillConfirm) == 0, "the same kill seq must not sting twice");

        s.latestKillSeq = 2;
        s.recentKills[1] = KillFeedEntry{ BOT_ID, LOCAL_ID, 1, 0 };   // someone else's kill
        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::KillConfirm) == 0, "only the local player's kills sting");
    }

    // --- a match/server reset drops latestKillSeq backward; kill-confirm must
    //     re-arm for the new match instead of staying silent until the seq
    //     climbs back past the previous match's final count
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        // Previous match: local player gets a kill near the end.
        s.latestKillSeq = 5;
        s.recentKills[4] = KillFeedEntry{ LOCAL_ID, BOT_ID, 0, 1 };
        uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::KillConfirm) == 1, "a kill just before the reset still stings once");

        // Match restart: server re-arms and latestKillSeq drops back to 0
        // while the players themselves stay in the snapshot.
        s.latestKillSeq = 0;
        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::KillConfirm) == 0, "the reset itself must not emit a stale sting");

        // New match: a fresh kill at a seq lower than the old lastShownKillSeq
        // must still sting, proving the layer re-armed instead of waiting to
        // climb back past the previous match's final count.
        s.latestKillSeq = 1;
        s.recentKills[0] = KillFeedEntry{ LOCAL_ID, BOT_ID, 0, 1 };
        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::KillConfirm) == 1, "kill-confirm re-arms for kills in the new match");
    }

    // --- a match/server reset also drops a present player's fireCounter back
    //     to 0; that must not be replayed as a burst of catch-up shots, and
    //     the player's next genuine shot after the reset must still register
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Bot(s).fireCounter = 50;
        Step(st, s, 0.016, ev);   // primes the bot at fireCounter == 50

        Bot(s).fireCounter = 0;   // reset: the player stays present in the snapshot
        uint8_t n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponFire) == 0,
              "a fireCounter reset must not be replayed as catch-up shots");

        Bot(s).fireCounter = 1;   // first genuine shot of the new match
        n = Step(st, s, 0.016, ev);
        CHECK(CountOf(ev, n, SoundId::WeaponFire) == 1,
              "the next genuine shot after a reset still fires exactly once");
    }

    // --- a stalled snapshot stream must not keep the footstep metronome going.
    //     Edge detection and the fire counter are inert on a frozen snapshot
    //     (no delta, no edge), but footsteps integrate real frame time against
    //     the frozen velocity, so a remote player who was running when the
    //     connection stalled would keep running forever.
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        Bot(s).velocity = DirectX::XMFLOAT3(8.0f, 0.0f, 0.0f);
        for (int i = 0; i < 10; ++i) Step(st, s, 0.016, ev);   // 160ms of live stream

        // The stream stalls: the caller keeps re-deriving from the SAME
        // snapshot every frame.  Call Derive directly so the tickId stays put.
        int early = 0;
        for (int i = 0; i < 40; ++i)      // 0.64s - crosses SNAPSHOT_STALE_SECONDS
            early += CountOf(ev, AudioEvents_Derive(st, s, 0.016, ev, 32), SoundId::Footstep);
        CHECK(early > 0, "a brief gap between snapshots must not cut footsteps off");

        int late = 0;
        for (int i = 0; i < 200; ++i)     // 3.2s more, all past the threshold
            late += CountOf(ev, AudioEvents_Derive(st, s, 0.016, ev, 32), SoundId::Footstep);
        CHECK(late == 0, "a stalled snapshot must stop producing footsteps");

        int resumed = 0;
        for (int i = 0; i < 60; ++i)
            resumed += CountOf(ev, Step(st, s, 0.016, ev), SoundId::Footstep);
        CHECK(resumed > 0, "footsteps resume as soon as snapshots start arriving again");
    }

    // --- overflow is dropped AND counted, so the caller can log it instead of
    //     the header promising a log line nothing ever writes
    {
        AudioEventState st{};
        Snapshot s = MakeSnapshot();
        Step(st, s, 0.016, ev);

        // Three simultaneous bot events, one slot to write them into.
        ++s.tickId;
        Bot(s).stateFlags |= NetStateFlags::IS_JUMPING;
        Bot(s).stateFlags |= NetStateFlags::IS_DEAD;
        Bot(s).health = 50;

        uint8_t dropped = 0xAA;
        const uint8_t n = AudioEvents_Derive(st, s, 0.016, ev, 1, &dropped);
        CHECK(n == 1,       "a full buffer stops at maxEvents");
        CHECK(dropped == 2, "and reports exactly how many events it had to drop");
    }

    std::printf(g_fail ? "\n%d FAILED\n" : "\nALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
