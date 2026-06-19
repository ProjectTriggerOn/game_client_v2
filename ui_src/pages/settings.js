// settings.js — self-contained IIFE; exposes only PageSettings = { onEnter, onExit }
//
// Realtime config keys (sensitivity, invert_y) apply immediately via
// game.setConfig (C++ subscribers fire on the spot). SAVE persists the user
// layer to user_settings.toml. BACK returns to the pause menu (game.backToPause).
// onEnter re-reads current values so the controls reflect persisted state.
//
// Listeners are delegated on the always-present #page-settings container (see
// pause.js note) — bound once at load, so they never depend on the injected
// controls existing yet and survive hot-reload re-injection. input/change/click
// all bubble, so delegation covers the slider, checkbox and buttons alike.

(function () {
    const root = document.getElementById('page-settings');

    root?.addEventListener('input', (e) => {
        if (e.target.id === 'set-sens') {
            const v = parseFloat(e.target.value);
            const out = document.getElementById('set-sens-val');
            if (out) out.textContent = v.toFixed(4);
            window.game?.setConfig?.('input.sensitivity', v);
        }
    });

    root?.addEventListener('change', (e) => {
        if (e.target.id === 'set-invert') {
            window.game?.setConfig?.('input.invert_y', e.target.checked);
        }
    });

    root?.addEventListener('click', (e) => {
        if (e.target.closest('#set-save')) window.game?.saveConfig?.();
        else if (e.target.closest('#set-back')) window.game?.backToPause?.();
    });

    // Re-read current values into the controls every time the page is shown.
    function refresh() {
        const g = window.game;
        if (!g || !g.getConfig) return;

        const sens = document.getElementById('set-sens');
        const out  = document.getElementById('set-sens-val');
        const cur  = g.getConfig('input.sensitivity');
        if (sens && cur !== null && cur !== undefined) {
            sens.value = cur;
            if (out) out.textContent = Number(cur).toFixed(4);
        }

        const invert = document.getElementById('set-invert');
        const inv = g.getConfig('input.invert_y');
        if (invert && inv !== null && inv !== undefined) invert.checked = !!inv;
    }

    function onEnter() { console.log('[PageSettings] enter'); refresh(); }
    function onExit()  { console.log('[PageSettings] exit'); }

    window.PageSettings = { onEnter, onExit };
})();
