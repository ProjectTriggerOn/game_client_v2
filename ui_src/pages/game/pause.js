// pause.js — self-contained IIFE; exposes only PagePause = { onEnter, onExit }
//
// The pause menu only changes GameState via game.* verbs; the active page and
// input level are derived from GameState by UIPolicy_Apply (C++). ESC is owned
// by gameplay (no key handler here) to avoid dual-consumer ping-pong (docs §5.3).
//
// Clicks use event delegation on the always-present #page-pause container
// (declared in index.html — only its innerHTML is injected/replaced). Binding
// once at load means we never depend on the injected buttons existing yet, so
// there is no "bind on first onEnter, retry if not ready" path. That old retry
// was unsound: Router.show short-circuits when the page is already current, so a
// failed first bind would not re-fire onEnter and the menu could stay dead.

(function () {
    const VERBS = {
        'pause-resume':   () => window.game?.resume?.(),
        'pause-settings': () => window.game?.openSettings?.(),
        'pause-quit':     () => window.game?.returnToTitle?.(),
    };

    document.getElementById('page-pause')?.addEventListener('click', (e) => {
        for (const id in VERBS) {
            if (e.target.closest('#' + id)) { VERBS[id](); return; }
        }
    });

    window.PagePause = {
        onEnter() { console.log('[PagePause] enter'); },
        onExit()  { console.log('[PagePause] exit'); },
    };
})();
