// title.js — self-contained IIFE; exposes only PageTitle = { onEnter, onExit }
//
// The title is the navigation ROOT of the menu flow. Its buttons drive scene/
// state transitions through the C++ bridge and the JS router:
//   PLAY     -> game.startLocalGame()  (SceneTransition_To(SCENE_GAME) — masked by the loading curtain)
//   SETTINGS -> Router.navigate('settings')  (nav-driven; settings BACK returns here)
//   QUIT     -> game.quit()  (PostQuitMessage)
//
// Listeners are delegated on the always-present #page-title container (like
// settings.js / pause.js) — bound once at load, so they don't depend on the
// injected markup existing yet and survive hot-reload re-injection.

(function () {
    const root = document.getElementById('page-title');

    root?.addEventListener('click', (e) => {
        if      (e.target.closest('#title-play'))     window.game?.startLocalGame?.();
        else if (e.target.closest('#title-settings')) window.Router?.navigate?.('settings');
        else if (e.target.closest('#title-quit'))     window.game?.quit?.();
    });

    // Fill the build-version stamp. game.* is bound by C++ in OnDOMReady, which
    // can race with router's async page injection — retry briefly if missing.
    function showVersion(attempt) {
        const g = window.game;
        if (!g || !g.getVersion) {
            if ((attempt || 0) < 20) setTimeout(() => showVersion((attempt || 0) + 1), 50);
            return;
        }
        const el = document.getElementById('title-version');
        if (el) el.textContent = 'build ' + g.getVersion();
    }

    function onEnter() {
        console.log('[PageTitle] enter');
        // Title is the nav root: clear any stale history so settings opened from
        // here always backs out to the title.
        window.Router?.resetNav?.();
        showVersion();
    }

    function onExit() {
        console.log('[PageTitle] exit');
    }

    window.PageTitle = { onEnter, onExit };
})();
