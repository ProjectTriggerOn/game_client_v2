//=============================================================================
// crosshair_gap.h — dynamic crosshair inner-gap math (spec §6.1).
// Client-only, pure functions, no state — extracted from Game_Draw so the
// gap behaviour is unit-testable (Game/tests/test_crosshair_gap.cpp).
// NOT mirrored to game_server: the server has no crosshair.
//=============================================================================
#pragma once

#include "../Network/recoil_math.h"

#include <cmath>

namespace Crosshair {

// rad -> px: tuned so bloomMax (1.5° ≈ 0.026 rad) lands ~24px at 1080p.
constexpr float PX_PER_RAD = 900.0f;
// The punch pulse opens the gap wider than the cone it represents — it is
// fire feedback, not a hit-uncertainty readout.
constexpr float PUNCH_PX_PER_RAD = PX_PER_RAD * 2.5f;
// Gap with a fully settled crosshair (no spread, no punch).
constexpr float GAP_BASE_PX = 4.0f;
// Visual cap applied AFTER smoothing — the punch pulse alone is ~470px at the
// 12° PUNCH_MAX (0.21 rad × 2250px/rad), which shoves the arms off-screen.
// Truncating a punch pulse at the cap is accepted: the visual bound wins, the
// crosshair never leaves the screen area (user ruling 2026-09-02).
constexpr float GAP_MAX_PX = 36.0f;

//-----------------------------------------------------------------------------
// GapTargetPixels — the inner gap the crosshair eases toward, derived from the
// shooter's recoil pool. Owning the choice of punch component here (rather
// than taking a pre-summed offset from the camera) is the point: the camera
// stores punch+shotKick, and shotKick must never reach the gap.
//-----------------------------------------------------------------------------
//   moveFactor: RecoilMath::MoveFactorFromVelocity(vx, vz) — the same 0..1
//     term the server feeds RecoilSpreadRadians, so the gap shows the real
//     cone rather than a stationary approximation of it.
inline float GapTargetPixels(const RecoilMath::RecoilState& rs,
                             uint8_t teamId, bool ads, float moveFactor)
{
    const float spreadRad =
        RecoilMath::RecoilSpreadRadians(teamId, ads, rs.bloomDeg, moveFactor);
    // DECAYING punch only. shotKick is a permanent aim offset the camera
    // already follows, so it carries no hit uncertainty; summing it in pinned
    // the gap at GAP_MAX from the first magazine onward (~59px of shotKick
    // after 30 RED rounds), which killed every other cue the gap conveys.
    const float punchRad = rs.punchPitch;
    return GAP_BASE_PX
         + spreadRad * PX_PER_RAD
         + std::fabs(punchRad) * PUNCH_PX_PER_RAD;
}

//-----------------------------------------------------------------------------
// ClampGap — pin the eased gap to the visual bound. Applied to the eased
// state itself so a recovery eases down from the visible GAP_MAX instead of
// stalling there while a hidden higher value decays.
//-----------------------------------------------------------------------------
inline float ClampGap(float gapPx)
{
    return gapPx > GAP_MAX_PX ? GAP_MAX_PX : gapPx;
}

} // namespace Crosshair
