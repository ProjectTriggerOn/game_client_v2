#include "scene.h"

#include "game.h"
#include "title.h"
#include "scene_ui_test.h"
#include "scene_editor.h"
#include "ui_manager.h"
#include "fade.h"

namespace
{
	scene g_CurrentScene = SCENE_GAME;// 現在のシーン
	scene g_NextScene = g_CurrentScene; // 次のシーン
	bool g_BootSceneLocked = false;     // boot scene can no longer change after Scene_Initialize
}

void Scene_SetBootScene(scene scene)
{
	if (g_BootSceneLocked) return;
	g_CurrentScene = scene;
	g_NextScene = scene;
}

void Scene_Initialize()
{
	g_BootSceneLocked = true;
	switch (g_CurrentScene)
	{
	case SCENE_TITLE:
		Title_Initialize();
		break;
	case SCENE_GAME:
		Game_Initialize();
		break;
	case SCENE_RESULT:

		break;
	case SCENE_UI_TEST:
		UITest_Initialize();
		break;
	case SCENE_EDITOR:
		SceneEditor_Initialize();
		break;
	default:
		break;
	}
}

void Scene_Finalize()
{
	switch (g_CurrentScene)
	{
	case SCENE_TITLE:
		Title_Finalize();
		break;
	case SCENE_GAME:
		Game_Finalize();
		break;
	case SCENE_RESULT:

		break;
	case SCENE_UI_TEST:
		UITest_Finalize();
		break;
	case SCENE_EDITOR:
		SceneEditor_Finalize();
		break;
	default:
		break;
	}
}

void Scene_Update(double elapsed_time)
{
	switch (g_CurrentScene)
	{
	case SCENE_TITLE:
		Title_Update(elapsed_time);
		break;
	case SCENE_GAME:
		Game_Update(elapsed_time);
		break;
	case SCENE_RESULT:

		break;
	case SCENE_UI_TEST:
		UITest_Update(elapsed_time);
		break;
	case SCENE_EDITOR:
		SceneEditor_Update(elapsed_time);
		break;
	default:
		break;
	}
}

void Scene_Draw()
{
	switch (g_CurrentScene)
	{
	case SCENE_TITLE:
		Title_Draw();
		break;
	case SCENE_GAME:

		Game_Draw();
		break;
	case SCENE_RESULT:

		break;
	case SCENE_UI_TEST:
		UITest_Draw();
		break;
	case SCENE_EDITOR:
		SceneEditor_Draw();
		break;
	default:
		break;
	}
}

void Scene_Refresh()
{
	if (g_CurrentScene != g_NextScene)
	{
		Scene_Finalize();
		g_CurrentScene = g_NextScene;
		Scene_Initialize();
	}
}

void Scene_Change(scene scene)
{
	g_NextScene = scene; // 現在のシーンを更新
}

scene Scene_GetCurrent()
{
	return g_CurrentScene;
}

void Restart_Game()
{
	Game_Finalize();
	Game_Initialize();
}

//=============================================================================
// Generic masked scene transition
//
// Sequences: cover (curtain fades to black) -> swap the scene while fully black
// (Scene_Refresh's heavy Game_Initialize hitch lands on a black frame) -> hold a
// minimum loading time -> reveal (curtain fades out). Drives the Ultralight
// #curtain overlay through UI::SetCurtain.
//
// All UI::SetCurtain calls happen from SceneTransition_Update (the main loop), so
// EvaluateScript is never re-entered from inside the JS bridge callback that kicks
// off the transition: SceneTransition_To only mutates native state (no script).
// While the screen should be covered, Update re-asserts Curtain.show() EVERY frame
// (idempotent) so the curtain survives a mid-transition DOM rebuild (hot reload).
//=============================================================================
namespace
{
	enum class TransitionPhase { Idle, Covering, Loading, Revealing };

	TransitionPhase g_TransPhase    = TransitionPhase::Idle;
	scene           g_TransTarget   = SCENE_TITLE;
	double          g_TransTimer    = 0.0;
	bool          (*g_TransReady)() = nullptr;  // optional load-complete gate (see SceneTransition_To)

	// TRANS_FADE_TIME must stay >= the #curtain CSS opacity transition (0.3s in
	// shared.css) so the curtain is fully opaque before the swap and fully clear
	// before input unlocks.
	constexpr double TRANS_FADE_TIME = 0.35;  // cover / reveal duration
	// Hold (loading) phase bounds. With the default null readiness gate the scene
	// is already fully loaded (synchronous Game_Initialize ran in the swap frame),
	// so the hold lasts exactly TRANS_MIN_HOLD — the original time-based behavior.
	// A real async gate holds the curtain until it reports ready, but never longer
	// than TRANS_MAX_HOLD (anti-hang safety net if the gate never fires).
	constexpr double TRANS_MIN_HOLD = 0.40;   // minimum loading-screen display (anti-flash)
	constexpr double TRANS_MAX_HOLD = 30.0;   // safety cap if a ready gate never fires
}

void SceneTransition_To(scene target, bool (*ready)())
{
	if (g_TransPhase != TransitionPhase::Idle) return;  // ignore re-entrant requests
	g_TransTarget = target;
	g_TransReady  = ready;   // null => purely time-based hold (synchronous load)
	g_TransPhase  = TransitionPhase::Covering;
	g_TransTimer  = 0.0;
	// Drop any in-flight native fade (e.g. the death/respawn red overlay) so it
	// can't bleed over the curtain or persist onto the next scene — the curtain
	// owns the transition visual. Pure native state, no EvaluateScript, so this is
	// safe to call from the JS bridge callback that invokes SceneTransition_To.
	Fade_Reset();
}

void SceneTransition_Update(double elapsed_time)
{
	if (g_TransPhase == TransitionPhase::Idle) return;

	// Keep the curtain asserted while it must stay opaque. Re-issued every frame
	// (Curtain.show() is idempotent) so a hot-reload DOM rebuild mid-transition
	// can't strand the curtain hidden. Always from the main loop — safe to
	// EvaluateScript here (cf. ShowPageDeferred re-entrancy rule).
	if (g_TransPhase == TransitionPhase::Covering || g_TransPhase == TransitionPhase::Loading)
		UI::SetCurtain(true);

	// Clamp the per-frame step so a single stutter frame can't outrun the CSS
	// curtain fade (which advances per rendered frame) and swap the scene before
	// the screen is actually black.
	g_TransTimer += (elapsed_time > 0.05 ? 0.05 : elapsed_time);

	switch (g_TransPhase)
	{
	case TransitionPhase::Covering:
		// Curtain is now fully opaque: swap behind it. Scene_Refresh (end of this
		// frame) runs the heavy init while the screen is black.
		if (g_TransTimer >= TRANS_FADE_TIME)
		{
			Scene_Change(g_TransTarget);
			g_TransPhase = TransitionPhase::Loading;
			g_TransTimer = 0.0;
		}
		break;

	case TransitionPhase::Loading:
		// Hold the loading screen until the target is ready AND the minimum hold has
		// elapsed (anti-flash), or the safety cap fires. The scene was swapped at the
		// end of the Covering frame, so a synchronous load is already complete here:
		// the default (null) gate is treated as ready, ending the hold at
		// TRANS_MIN_HOLD (the original behavior). A real async gate passed to
		// SceneTransition_To keeps the curtain up until loading actually finishes.
		{
			const bool ready = (g_TransReady == nullptr) || g_TransReady();
			if ((ready && g_TransTimer >= TRANS_MIN_HOLD) || g_TransTimer >= TRANS_MAX_HOLD)
			{
				UI::SetCurtain(false);   // main loop — safe
				g_TransPhase = TransitionPhase::Revealing;
				g_TransTimer = 0.0;
			}
		}
		break;

	case TransitionPhase::Revealing:
		if (g_TransTimer >= TRANS_FADE_TIME)
			g_TransPhase = TransitionPhase::Idle;
		break;

	default:
		break;
	}
}

bool SceneTransition_IsActive()
{
	return g_TransPhase != TransitionPhase::Idle;
}
