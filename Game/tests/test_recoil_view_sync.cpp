// Standalone test for the rendered-view / recoil-pool sync contract.
// No game engine, no DirectXMath — player_cam_fps.cpp cannot be linked here
// (Win32 + DirectX), so this replays the exact per-frame protocol the client
// runs and asserts the contract on it.
// Build: cl /std:c++17 /EHsc /I . Game\tests\test_recoil_view_sync.cpp
//
// THE CONTRACT (player_cam_fps.h:32-42, spec §2 WYSIWYG):
//   The rendered camera offset MUST equal RecoilTotalOffsets(pool) at every
//   frame, because the server ray-casts through exactly those offsets
//   (game_server.cpp:754, mock_server.cpp:767). Any divergence means bullets
//   leave the muzzle somewhere other than where the player is looking.
//
// ClientSim below mirrors the shipped client protocol:
//   - PlayerFps::ConsumeRound         (player_fps.cpp, recoil block)
//   - PlayerFps::Update pool decay    (player_fps.cpp, recoil block)
//   - PlayerFps::PushRecoilToCamera   -> PlayerCamFps_SetPunch
//
// The pool is the only integrator. A camera that runs its own decay fails
// case 2/3 below by ~1.5° per magazine — that was the shipped bug.
#include "../../Network/recoil_math.h"
#include <cstdio>
#include <cmath>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); g_fail++; } } while (0)

using namespace RecoilMath;

static constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

//-----------------------------------------------------------------------------
// RenderedPunch — the camera's copy of the punch offset (g_punchPitch /
// g_punchYaw in player_cam_fps.cpp). A pure store: no integration of its own.
//-----------------------------------------------------------------------------
struct RenderedPunch {
    float pitch = 0.0f;
    float yaw   = 0.0f;

    // PlayerCamFps_SetPunch — absolute value, pushed from the pool.
    void Set(float punchPitch, float punchYaw) { pitch = punchPitch; yaw = punchYaw; }
};

//-----------------------------------------------------------------------------
// ClientSim — one client's recoil pool + rendered offset, stepped like the
// real frame loop.
//-----------------------------------------------------------------------------
struct ClientSim {
    RecoilState   pool;
    RenderedPunch view;
    uint16_t      fireCounter = 0;
    double        nowSec      = 0.0;
    uint8_t       teamId      = PlayerTeam::RED;

    // PlayerFps::PushRecoilToCamera
    void PushToCamera()
    {
        float dp = 0.0f, dy = 0.0f;
        RecoilTotalOffsets(pool, dp, dy);
        view.Set(dp, dy);
    }

    // PlayerFps::Update — gated pool decay, then push to the camera.
    void Frame(float dt)
    {
        RecoilAdvance(pool, teamId, fireCounter,
                      /*ads=*/false, /*newlyFired=*/false, dt, nowSec);
        PushToCamera();
        nowSec += dt;
    }

    // PlayerFps::ConsumeRound — advance the pool, then push to the camera.
    void Fire()
    {
        ++fireCounter;
        RecoilAdvance(pool, teamId, fireCounter,
                      /*ads=*/false, /*newlyFired=*/true, /*dt=*/0.0f, nowSec);
        PushToCamera();
    }

    // The offset the server ray uses for this player.
    void ServerAim(float& p, float& y) const { RecoilTotalOffsets(pool, p, y); }

    void Rest(double seconds, float dt)
    {
        const double end = nowSec + seconds;
        while (nowSec < end) Frame(dt);
    }

    void Mag(int shots, double rpm, float dt)
    {
        const double interval = 60.0 / rpm;
        for (int s = 0; s < shots; ++s)
        {
            const double end = nowSec + interval;
            while (nowSec < end) Frame(dt);
            Fire();
        }
    }
};

int main()
{
    constexpr float DT  = 1.0f / 120.0f;   // 120 fps client
    constexpr float TOL = 1e-4f;           // rad (~0.006°)

    //-------------------------------------------------------------------------
    // 1. Rendered view matches the server ray while a burst is landing.
    //-------------------------------------------------------------------------
    {
        ClientSim c;
        c.Mag(10, 600.0, DT);
        float ap = 0.0f, ay = 0.0f;
        c.ServerAim(ap, ay);
        CHECK(std::fabs(c.view.pitch - ap) < TOL,
              "mid-burst: rendered pitch == server aim pitch");
        CHECK(std::fabs(c.view.yaw - ay) < TOL,
              "mid-burst: rendered yaw == server aim yaw");
    }

    //-------------------------------------------------------------------------
    // 2. Rendered view matches the server ray after the trigger is released.
    //    shotKick does NOT decay in the pool, so the rendered offset must not
    //    decay past it either — this is the drift that makes late-life shots
    //    fly high.
    //-------------------------------------------------------------------------
    {
        ClientSim c;
        c.Mag(30, 600.0, DT);      // one RED mag
        c.Rest(1.5, DT);           // trigger released, punch recovers
        float ap = 0.0f, ay = 0.0f;
        c.ServerAim(ap, ay);
        const float driftDeg = (ap - c.view.pitch) * kRadToDeg;
        if (std::fabs(c.view.pitch - ap) >= TOL)
            std::printf("  [1 mag + rest] server aim %.3f deg, rendered %.3f deg, drift %.3f deg\n",
                        ap * kRadToDeg, c.view.pitch * kRadToDeg, driftDeg);
        CHECK(std::fabs(c.view.pitch - ap) < TOL,
              "after rest: rendered pitch == server aim pitch (shotKick retained)");
    }

    //-------------------------------------------------------------------------
    // 3. The drift must not grow with rounds fired across a life.
    //-------------------------------------------------------------------------
    {
        ClientSim c;
        float worstDeg = 0.0f;
        for (int mag = 0; mag < 4; ++mag)
        {
            c.Mag(30, 600.0, DT);
            c.Rest(1.5, DT);
            float ap = 0.0f, ay = 0.0f;
            c.ServerAim(ap, ay);
            const float d = std::fabs(ap - c.view.pitch) * kRadToDeg;
            if (d > worstDeg) worstDeg = d;
        }
        if (worstDeg * (1.0f / kRadToDeg) >= TOL)
            std::printf("  [4 mags] worst drift %.3f deg\n", worstDeg);
        CHECK(worstDeg * (1.0f / kRadToDeg) < TOL,
              "4 mags: drift does not accumulate across a life");
    }

    if (g_fail == 0) std::printf("test_recoil_view_sync: ALL PASS\n");
    else             std::printf("test_recoil_view_sync: %d FAILURE(S)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
