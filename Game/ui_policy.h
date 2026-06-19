#pragma once
#ifndef UI_POLICY_H
#define UI_POLICY_H

// Single owner of UI InteractiveLevel + active Page.
//
// Two page-switching modes, chosen per scene:
//
//   - State-driven (SCENE_GAME): page + InteractiveLevel are a pure function of
//     GameState, re-derived every frame and forced on every transition. Menus
//     change GameState only; the page follows. (hud / pause / settings)
//
//   - Navigation-driven (other scenes — ui_test today, lobby later): the scene's
//     ENTRY page + a fixed InteractiveLevel are set once on scene-enter; sub-page
//     navigation (e.g. lobby home ↔ armory ↔ profile) is then owned by JS via
//     Router.navigate/back. C++ does not force a page after entry.
//
// This is the same declarative pattern as MousePolicy_Apply (docs §5.5 / §5.6):
// nothing else may call UI::SetInteractiveLevel or push a state-driven page.
//
// Call once per frame after Scene_Update (next to MousePolicy_Apply).
void UIPolicy_Apply();

// The page that should be active right now for the current (scene, GameState):
//   - state-driven scene      → the authoritative page for the current GameState
//   - navigation-driven scene → that scene's ENTRY page
// Used by the boot/reload path (Slice F getBootPage) so the View comes up on the
// correct page. Returns a stable string literal.
const char* UIPolicy_DerivePage();

#endif
