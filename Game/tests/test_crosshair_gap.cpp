// Standalone unit test for the dynamic crosshair inner gap (spec §6.1).
// Build: cl /std:c++17 /EHsc /I . Game\tests\test_crosshair_gap.cpp
//
// The gap is driven by Game_Draw from the shooter's recoil pool plus the
// movement term (MoveFactorFromVelocity) — the same inputs the server feeds
// RecoilSpreadRadians. It uses the DECAYING punch only.
// It must NOT see shotKick: that is a permanent aim offset the camera already
// follows, so it carries no hit uncertainty, and feeding it in pins the gap at
// GAP_MAX for the rest of the player's life.
#include "../crosshair_gap.h"
#include "../../Network/recoil_math.h"
#include <cstdio>
#include <cmath>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); g_fail++; } } while (0)

using namespace RecoilMath;

// One RED magazine fired, then the trigger released long enough for punch and
// bloom to fully decay. shotKick survives; punch and bloom do not.
static RecoilState RestedAfterOneMag()
{
    RecoilState rs;
    double now = 0.0;
    constexpr float DT = 1.0f / 120.0f;
    const double interval = 60.0 / 600.0;   // 600 RPM
    for (uint16_t shot = 1; shot <= 30; ++shot)
    {
        const double end = now + interval;
        while (now < end)
        {
            RecoilAdvance(rs, PlayerTeam::RED, shot, false, false, DT, now);
            now += DT;
        }
        RecoilAdvance(rs, PlayerTeam::RED, shot, false, true, 0.0f, now);
    }
    // Trigger released long enough for a PUNCH_MAX-capped punch to fully
    // recover: 4Hz decay needs ~3s to take 12° below a tenth of a pixel
    // (1.5s still leaves 0.03° ≈ 3.4px of real, correctly-decaying punch).
    const double end = now + 3.0;
    while (now < end)
    {
        RecoilAdvance(rs, PlayerTeam::RED, 30, false, false, DT, now);
        now += DT;
    }
    return rs;
}

int main()
{
    //-------------------------------------------------------------------------
    // 1. A settled crosshair sits at base + the HIP cone, well under the cap.
    //-------------------------------------------------------------------------
    {
        const RecoilState fresh;
        const float gap = Crosshair::GapTargetPixels(fresh, PlayerTeam::RED, false, 0.0f);
        CHECK(gap > 20.0f && gap < 26.0f, "fresh spawn: settled gap ~23px");
        CHECK(gap < Crosshair::GAP_MAX_PX, "fresh spawn: settled gap under the cap");
    }

    //-------------------------------------------------------------------------
    // 2. After a magazine, a RESTED crosshair returns to the same settled gap.
    //    shotKick must not leak into it (regression: it pinned the gap at
    //    GAP_MAX from the first magazine onward, killing every other cue).
    //-------------------------------------------------------------------------
    {
        const RecoilState rs = RestedAfterOneMag();
        const RecoilState spawn;
        const float fresh  = Crosshair::GapTargetPixels(spawn, PlayerTeam::RED, false, 0.0f);
        const float rested = Crosshair::GapTargetPixels(rs, PlayerTeam::RED, false, 0.0f);
        if (std::fabs(rested - fresh) >= 1.0f)
            std::printf("  [1 mag + rest] fresh %.1fpx, rested %.1fpx (shotKick %.3f deg)\n",
                        fresh, rested, rs.shotKickPitch * 180.0f / 3.14159265f);
        CHECK(std::fabs(rested - fresh) < 1.0f,
              "after a mag + rest: gap returns to the settled value");
        CHECK(Crosshair::ClampGap(rested) < Crosshair::GAP_MAX_PX,
              "after a mag + rest: gap is not pinned at the cap");
    }

    //-------------------------------------------------------------------------
    // 3. The gap still reacts to a live burst — the punch pulse must open it.
    //-------------------------------------------------------------------------
    {
        RecoilState rs;
        for (uint16_t shot = 1; shot <= 5; ++shot)
            RecoilAdvance(rs, PlayerTeam::RED, shot, false, true, 0.0f, 0.0);
        const RecoilState spawn;
        const float firing  = Crosshair::GapTargetPixels(rs, PlayerTeam::RED, false, 0.0f);
        const float settled = Crosshair::GapTargetPixels(spawn, PlayerTeam::RED, false, 0.0f);
        CHECK(firing > settled + 5.0f, "mid-burst: punch pulse opens the gap");
    }

    //-------------------------------------------------------------------------
    // 4. ADS pulls the gap in (0.2 deg cone vs 1.2 deg).
    //-------------------------------------------------------------------------
    {
        const RecoilState spawn;
        CHECK(Crosshair::GapTargetPixels(spawn, PlayerTeam::RED, true, 0.0f)
                < Crosshair::GapTargetPixels(spawn, PlayerTeam::RED, false, 0.0f),
              "ADS: settled gap tighter than HIP");
    }

    //-------------------------------------------------------------------------
    // 5. Movement widens the gap — the spec's HIP ×1.5 / ADS ×1.3 penalty,
    //    which was dead code until the call sites started passing moveFactor.
    //-------------------------------------------------------------------------
    {
        const RecoilState spawn;
        const float still   = Crosshair::GapTargetPixels(spawn, PlayerTeam::RED, false, 0.0f);
        const float running = Crosshair::GapTargetPixels(spawn, PlayerTeam::RED, false, 1.0f);
        CHECK(running > still + 5.0f, "HIP: running opens the gap wider than standing");

        const float adsStill   = Crosshair::GapTargetPixels(spawn, PlayerTeam::RED, true, 0.0f);
        const float adsRunning = Crosshair::GapTargetPixels(spawn, PlayerTeam::RED, true, 1.0f);
        CHECK(adsRunning > adsStill, "ADS: running still widens, just less");
        CHECK((adsRunning - adsStill) < (running - still),
              "ADS movement penalty is smaller than HIP (x1.3 vs x1.5)");
    }

    if (g_fail == 0) std::printf("test_crosshair_gap: ALL PASS\n");
    else             std::printf("test_crosshair_gap: %d FAILURE(S)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
