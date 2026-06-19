#pragma once

// Slice F — UI hot reload (docs §10).
//
// A background thread watches the ui_src directory via ReadDirectoryChangesW and
// queues changed file paths. The main thread drains the queue once per frame
// (Poll, called from UI::Render) and acts by extension:
//   - .css        → UI::ReloadStyles (non-destructive: re-fetch the stylesheet)
//   - .html / .js → UI::ReloadView  (full View::Reload; boot page restored via
//                                    game.getBootPage so we land back on the
//                                    page that matches the current GameState)
//
// Debug-only. In Release every entry point is an empty stub (see ui_hot_reload.cpp),
// so callers need not guard each call — though we still guard the wiring in
// ui_manager to avoid a per-frame no-op call.

namespace UI {
namespace HotReload {

// Start watching `watchDir` (the resolved ui_src root). No-op if already running
// or in Release.
void Start(const char* watchDir);

// Stop the watcher thread and release the directory handle. Safe to call when
// not running.
void Stop();

// Main-thread pump: drain queued file changes, debounce, and dispatch to the
// UI::Reload* helpers. Call once per frame at the top of UI::Render.
void Poll();

}  // namespace HotReload
}  // namespace UI
