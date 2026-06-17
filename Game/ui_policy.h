#pragma once
#ifndef UI_POLICY_H
#define UI_POLICY_H

// Single owner of UI InteractiveLevel + active Page, derived declaratively from
// (scene, GameState) each frame — the same pattern as MousePolicy_Apply
// (docs §5.5 / §5.6). Nothing else may call UI::SetInteractiveLevel or push a
// state-driven page; menus change GameState, and the level + page follow.
//
// Derivation (source of truth = GameState within SCENE_GAME):
//   scene != SCENE_GAME            → Interactive, page left to the sandbox
//   scene == SCENE_GAME:
//     GameState PAUSE              → Modal,    page "pause"
//     GameState SETTING            → Modal,    page "settings"
//     otherwise (PLAY/READY/...)   → Display,  page "hud"
//
// Page is switched only on transition; level is reasserted every frame
// (SetInteractiveLevel is idempotent).
void UIPolicy_Apply();

#endif
