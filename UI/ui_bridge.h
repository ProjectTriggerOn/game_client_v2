#pragma once

namespace ultralight { class View; }

namespace UI {
namespace Bridge {

// Bind the `game.*` API into the View's JS context, and remember the view for
// the push helpers below. Must be called from OnDOMReady (the JSContext resets
// on every navigation, so ui_manager's load listener re-invokes this after each
// reload). API contract: docs/ultralight_integration.md §8.1.
void Register(ultralight::View* view);

// Forget the cached view (call on Finalize). Push helpers become no-ops.
void Unbind();

// C++ → JS push (docs §8.2). Each fetches window.on*Changed fresh and calls it;
// no-op if the handler isn't defined (e.g. HUD page not loaded).
void PushHealth(int current, int maxHp);
void PushAmmo(int current, int reserve);

// Scoring pushes (same fresh-lookup contract as above).
void PushScores(int red, int blue);
void PushMatchTimer(float secondsRemaining);
void PushKillFeed(int killerId, int victimId, int killerTeam, int victimTeam);
void PushScoreboard(const char* json);
void PushScoreboardVisible(bool visible);
void PushMatchResult(const char* json);

// Display-settings revert countdown → window.onDisplayRevertTick(secondsLeft).
void PushDisplayRevertTick(int secondsLeft);

}  // namespace Bridge
}  // namespace UI
