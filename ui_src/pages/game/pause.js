// pause.js — self-contained IIFE; exposes only PagePause = { onEnter, onExit }
//
// The pause menu only changes GameState via game.* verbs; the active page and
// input level are derived from GameState by UIPolicy_Apply (C++). ESC is owned
// by gameplay (no key handler here) to avoid dual-consumer ping-pong (docs §5.3).

(function () {
    let bound = false;

    function bindOnce() {
        if (bound) return;
        const resume = document.getElementById('pause-resume');
        if (!resume) return;            // markup not injected yet; retry on next onEnter
        bound = true;

        resume.addEventListener('click', () => window.game?.resume?.());
        document.getElementById('pause-settings')
            ?.addEventListener('click', () => window.game?.openSettings?.());
        document.getElementById('pause-quit')
            ?.addEventListener('click', () => window.game?.returnToTitle?.());
    }

    function onEnter() { console.log('[PagePause] enter'); bindOnce(); }
    function onExit()  { console.log('[PagePause] exit'); }

    window.PagePause = { onEnter, onExit };
})();
