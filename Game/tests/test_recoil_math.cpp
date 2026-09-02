// Standalone unit test for recoil_math.h. No game engine, no DirectXMath.
// Build: g++ -std=c++17 -Wall -Wextra -I . Game/tests/test_recoil_math.cpp -o _test_recoil_math.exe && ./_test_recoil_math.exe
#include "../../Network/recoil_math.h"
#include <cstdio>
#include <cmath>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); g_fail++; } } while (0)
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

using namespace RecoilMath;

int main()
{
    //-------------------------------------------------------------------------
    // 1. Determinism: same (teamId, fireCounter) → identical punch advance
    //-------------------------------------------------------------------------
    {
        RecoilState a, b;
        RecoilAdvance(a, PlayerTeam::RED, 7, false, true, 0.0f, 0.0);
        RecoilAdvance(b, PlayerTeam::RED, 7, false, true, 0.0f, 0.0);
        CHECK(Near(a.punchPitch, b.punchPitch) && Near(a.punchYaw, b.punchYaw),
              "same input -> identical punch");
    }

    //-------------------------------------------------------------------------
    // 2. PATTERN_LEN wraparound: shot 31 == shot 1 (fireCounter 31 → idx 0)
    //-------------------------------------------------------------------------
    {
        RecoilState a, b;
        RecoilAdvance(a, PlayerTeam::RED, 1, false, true, 0.0f, 0.0);
        RecoilAdvance(b, PlayerTeam::RED, 31, false, true, 0.0f, 0.0);
        CHECK(Near(a.punchPitch, b.punchPitch), "pattern wraps at PATTERN_LEN+1");
    }

    //-------------------------------------------------------------------------
    // 3. u16 fireCounter wraparound: 65535 → next pattern idx is (65535-1)%30,
    //    then (65536-1)%30 == 65535%30 — must not crash / must stay in range.
    //    Also shotKick must keep accumulating monotonically.
    //-------------------------------------------------------------------------
    {
        RecoilState rs;
        float prevKick = -1.0f;
        bool monotonic = true;
        for (uint32_t fc = 65530u; fc <= 65540u; ++fc)
        {
            const uint16_t fc16 = static_cast<uint16_t>(fc & 0xFFFFu);
            RecoilState before = rs;
            RecoilAdvance(rs, PlayerTeam::BLUE, fc16, false, true, 0.0f, 0.0);
            if (rs.shotKickPitch < before.shotKickPitch) monotonic = false;
            prevKick = rs.shotKickPitch;
        }
        CHECK(monotonic, "shotKick accumulates monotonically across u16 wrap");
        (void)prevKick;
    }

    //-------------------------------------------------------------------------
    // 4. Punch envelope: first shot weaker than 5th shot of the same burst
    //-------------------------------------------------------------------------
    {
        RecoilState first, fifth;
        RecoilAdvance(first, PlayerTeam::RED, 1, false, true, 0.0f, 0.0);
        for (uint16_t fc = 1; fc <= 5; ++fc) RecoilAdvance(fifth, PlayerTeam::RED, fc, false, true, 0.0f, 0.0);
        CHECK(first.punchPitch < fifth.punchPitch / 4.5f,
              "first shot punch ramped below 5th-shot accumulation");
    }

    //-------------------------------------------------------------------------
    // 5. ADS steadier: ADS punch per shot = 0.8 × HIP punch
    //-------------------------------------------------------------------------
    {
        RecoilState hip, ads;
        RecoilAdvance(hip, PlayerTeam::BLUE, 3, false, true, 0.0f, 0.0);
        RecoilAdvance(ads, PlayerTeam::BLUE, 3, true, true, 0.0f, 0.0);
        CHECK(Near(ads.punchPitch, hip.punchPitch * 0.8f), "ADS punch 0.8x HIP");
    }

    //-------------------------------------------------------------------------
    // 6. Decay convergence: any initial punch decays to ~0; frame-rate
    //    independence: 10×dt in one step == 10 steps of dt (to first order —
    //    exact identity only when dt→0, so assert loose equality).
    //-------------------------------------------------------------------------
    {
        // decayHz*dt is fixed at 2.5 (exp(-2.5) ≈ 0.082 < 0.1); dt is derived
        // from the spec so the assertion holds for any decayHz tuning.
        const float decayHz = RecoilConfig::SpecForTeam(PlayerTeam::RED).decayHz;
        const float totalDt = 2.5f / decayHz;

        RecoilState once; once.punchPitch = 1.0f;
        RecoilAdvance(once, PlayerTeam::RED, 1, false, false, totalDt, 0.0);
        CHECK(once.punchPitch < 0.1f, "punch decays >90% after decayHz*dt=2.5");

        RecoilState fine;
        fine.punchPitch = 1.0f;
        for (int i = 0; i < 50; ++i)
            RecoilAdvance(fine, PlayerTeam::RED, 1, false, false, totalDt / 50.0f, 0.0);
        CHECK(Near(once.punchPitch, fine.punchPitch, 0.02f),
              "decay is dt-granularity independent (within discretization error)");
    }

    //-------------------------------------------------------------------------
    // 7. Bloom: grows per shot up to a cap, shrinks with decay
    //-------------------------------------------------------------------------
    {
        RecoilState rs;
        for (int i = 1; i <= 40; ++i)
            RecoilAdvance(rs, PlayerTeam::RED, static_cast<uint16_t>(i), false, true, 0.0f, 0.0);
        CHECK(Near(rs.bloomDeg, RecoilConfig::RED_SPEC.bloomMaxDeg),
              "bloom saturates at bloomMaxDeg");

        RecoilState rs2 = rs;
        // decayHz*dt fixed at 5.0 (exp(-5.0) ≈ 0.0067 → bloom < 1% of max);
        // dt derived from the spec so the assertion holds for any decayHz.
        // rs2 fired at nowSec=0; a nowSec past 0.25 clears the suspend window.
        RecoilAdvance(rs2, PlayerTeam::RED, 41, false, false,
                      5.0f / RecoilConfig::SpecForTeam(PlayerTeam::RED).decayHz, 1.0);
        CHECK(rs2.bloomDeg < rs.bloomDeg * 0.01f, "bloom decays with punch");
    }

    //-------------------------------------------------------------------------
    // 8. Cone offset: bounded by spread; zero spread → zero offset; spread
    //    scales offsets; deterministic for same fireCounter
    //-------------------------------------------------------------------------
    {
        float dp = 0, dy = 0;
        RecoilConeOffset(0.0f, 123, dp, dy);
        CHECK(Near(dp, 0.0f) && Near(dy, 0.0f), "zero spread -> zero offset");

        float maxSeen = 0.0f;
        for (uint16_t fc = 1; fc <= 200; ++fc)
        {
            float a = 0, b = 0;
            RecoilConeOffset(0.05f, fc, a, b);
            const float mag = std::sqrt(a * a + b * b);
            maxSeen = std::fmax(maxSeen, mag);
            CHECK(mag <= 0.05f + 1e-6f, "offset magnitude within cone");
        }
        CHECK(maxSeen > 0.02f, "200 shots cover a decent cone radius");

        float p1, y1, p2, y2;
        RecoilConeOffset(0.05f, 77, p1, y1);
        RecoilConeOffset(0.05f, 77, p2, y2);
        CHECK(Near(p1, p2) && Near(y1, y2), "cone offset deterministic");
    }

    //-------------------------------------------------------------------------
    // 9. WYSIWYG totals: punch + kick add up in RecoilTotalOffsets
    //-------------------------------------------------------------------------
    {
        RecoilState rs;
        rs.punchPitch = 0.010f; rs.punchYaw = 0.004f;
        rs.shotKickPitch = 0.001f; rs.shotKickYaw = 0.0005f;
        float dp = 0, dy = 0;
        RecoilTotalOffsets(rs, dp, dy);
        CHECK(Near(dp, 0.011f) && Near(dy, 0.0045f), "total offsets = punch + kick");
    }

    //-------------------------------------------------------------------------
    // 10. Hash01: [0,1) range and determinism
    //-------------------------------------------------------------------------
    {
        bool inRange = true;
        for (uint32_t i = 0; i < 1000; ++i)
        {
            const float h = Hash01(static_cast<uint16_t>(i & 0xFFFFu));
            if (!(h >= 0.0f && h < 1.0f)) inRange = false;
        }
        CHECK(inRange, "Hash01 within [0,1)");
        CHECK(Near(Hash01(4242), Hash01(4242)), "Hash01 deterministic");
    }

    //-------------------------------------------------------------------------
    // 11. Firing suspends decay (COD burst ramp): 5 shots spaced 0.1s apart —
    //     each inside the FIRE_SUSPEND_DECAY_S window — accumulate the FULL
    //     per-shot punch. No exp(-decayHz*dt) recovery runs while the trigger
    //     is down, so a burst climbs instead of plateauing at a steady state.
    //-------------------------------------------------------------------------
    {
        const RecoilConfig::WeaponSpec& w = RecoilConfig::SpecForTeam(PlayerTeam::RED);
        RecoilState rs;
        double t = 1000.0;                 // arbitrary monotonic clock
        float expectRad = 0.0f;
        for (uint16_t fc = 1; fc <= 5; ++fc)
        {
            const uint16_t idx = static_cast<uint16_t>((fc - 1u) % RecoilConfig::PATTERN_LEN);
            expectRad += w.punchPitchDeg * kDegToRad * PunchEnvelope(idx); // HIP, no decay
            RecoilAdvance(rs, PlayerTeam::RED, fc, false, true, 0.1f, t);
            t += 0.1;
        }
        CHECK(Near(rs.punchPitch, expectRad, 1e-4f),
              "firing suspends decay — full 5-shot punch accumulates");
    }

    //-------------------------------------------------------------------------
    // 12. Recovery after the trigger is released: while nowSec is still inside
    //     the 0.25s suspend window nothing decays; once it clears, the same
    //     exp(-decayHz*dt) recovery as before resumes.
    //-------------------------------------------------------------------------
    {
        RecoilState rs;
        double t = 1000.0;
        for (uint16_t fc = 1; fc <= 5; ++fc)
        {
            RecoilAdvance(rs, PlayerTeam::RED, fc, false, true, 0.0f, t);
            t += 0.1;
        }
        const float afterBurst = rs.punchPitch;      // last shot at t-0.1
        CHECK(afterBurst > 0.1f, "burst leaves a sizeable punch");

        // 0.2s after the last shot — still inside the window: no decay.
        RecoilAdvance(rs, PlayerTeam::RED, 6, false, false, 0.1f, t + 0.1f);
        CHECK(Near(rs.punchPitch, afterBurst, 1e-6f),
              "no decay while still inside the FIRE_SUSPEND_DECAY window");

        // 0.6s after the last shot — window cleared: decay resumes (dt=0.5,
        // exp(-4*0.5)=0.135 → >75% gone).
        RecoilAdvance(rs, PlayerTeam::RED, 6, false, false, 0.5f, t + 1.0f);
        CHECK(rs.punchPitch < afterBurst * 0.25f,
              "punch recovers once nowSec clears the suspend window");
    }

    //-------------------------------------------------------------------------
    // 13. PUNCH_MAX cap: a full 30-round dump with decay suspended must not
    //     drive pitch past the 12° ceiling (a mag dump can't point at the sky).
    //-------------------------------------------------------------------------
    {
        RecoilState rs;
        for (uint16_t fc = 1; fc <= 30; ++fc)
            RecoilAdvance(rs, PlayerTeam::RED, fc, false, true, 0.0f, 0.0);
        const float capRad = RecoilConfig::PUNCH_MAX_DEG * kDegToRad;
        CHECK(rs.punchPitch <= capRad + 1e-6f, "punch pitch capped at PUNCH_MAX_DEG");
        CHECK(Near(rs.punchPitch, capRad, 1e-4f), "30-round dump sits AT the cap");
    }

    if (g_fail == 0) std::printf("test_recoil_math: ALL PASS\n");
    else             std::printf("test_recoil_math: %d FAILURES\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
