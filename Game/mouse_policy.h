#pragma once
#ifndef MOUSE_POLICY_H
#define MOUSE_POLICY_H

// Single owner of the hardware cursor state (mode / visibility / MSLogger UI
// flag). Nothing else in the codebase may call Mouse_SetMode / Mouse_SetVisible
// / MSLogger_SetUIMode directly (sole exception: the WM_CLOSE handler, which
// runs a modal MessageBox while the frame loop is blocked).
//
// Everything else just changes *state* (scene, GameState, debug-cam flag, UI
// modal level); MousePolicy_Apply derives the cursor from that state once per
// frame. Both Mouse_SetMode and Mouse_SetVisible are idempotent, so the
// per-frame reapply is free — and any direct mutation (e.g. by the WM_CLOSE
// handler) self-heals on the next frame.
//
// Decision table:
//   scene != SCENE_GAME                    → free cursor (ABSOLUTE + visible)
//   scene == SCENE_GAME:
//     Game_WantsUICursor() (settings/debug) → free cursor
//     UI::IsModalActive()  (Slice E pause)  → free cursor
//     otherwise (gameplay)                  → RELATIVE + hidden
void MousePolicy_Apply();

#endif
