// settings.js — self-contained IIFE; exposes only PageSettings = { onEnter, onExit }
//
// Realtime config keys (sensitivity, invert_y) apply immediately via
// game.setConfig (C++ subscribers fire on the spot). SAVE persists the user
// layer to user_settings.toml. BACK returns to the pause menu (game.backToPause).
// onEnter re-reads current values so the controls reflect persisted state.

(function () {
    let bound = false;

    function bindOnce() {
        if (bound) return;
        const sens = document.getElementById('set-sens');
        if (!sens) return;              // markup not injected yet
        bound = true;

        sens.addEventListener('input', () => {
            const v = parseFloat(sens.value);
            const out = document.getElementById('set-sens-val');
            if (out) out.textContent = v.toFixed(4);
            window.game?.setConfig?.('input.sensitivity', v);
        });

        document.getElementById('set-invert')?.addEventListener('change', (e) => {
            window.game?.setConfig?.('input.invert_y', e.target.checked);
        });

        document.getElementById('set-save')
            ?.addEventListener('click', () => window.game?.saveConfig?.());
        document.getElementById('set-back')
            ?.addEventListener('click', () => window.game?.backToPause?.());
    }

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

    function onEnter() { console.log('[PageSettings] enter'); bindOnce(); refresh(); }
    function onExit()  { console.log('[PageSettings] exit'); }

    window.PageSettings = { onEnter, onExit };
})();
