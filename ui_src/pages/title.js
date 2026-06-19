// title.js — self-contained IIFE; exposes only PageTitle = { onEnter, onExit }
//
// Slice C: onEnter wires up the verification widgets (button counter, Esc key,
// scroll list population).
// Note: page markup is injected by router.js after a fetch, so DOM queries must
// happen inside onEnter — not at IIFE top level (the div is still empty when
// this script loads).

(function () {
    let clickCount = 0;
    let bound = false;

    function bindOnce() {
        if (bound) return;
        bound = true;

        // Button: click counter (verifies MouseDown/Up + click synthesis)
        const btn = document.getElementById('btn-test');
        btn?.addEventListener('click', () => {
            clickCount++;
            btn.textContent = 'CLICK ME — ' + clickCount;
            console.log('[PageTitle] button clicked:', clickCount);
        });

        // Scroll list: fill 30 rows (verifies ScrollEvent)
        const list = document.getElementById('scroll-test');
        if (list && list.children.length === 0) {
            for (let i = 1; i <= 30; i++) {
                const row = document.createElement('div');
                row.className = 'row';
                row.textContent = 'scroll row ' + i;
                list.appendChild(row);
            }
        }

        // Esc counter (switches to game.setState from Slice D on).
        // Note: e.key relies on the C++ side filling key_identifier
        // (ui_manager.cpp ProcessInput); keyCode is the fallback so this keeps
        // working even where Ultralight's e.key mapping is incomplete.
        let escCount = 0;
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' || e.keyCode === 27) {
                escCount++;
                const hint = document.querySelector('#page-title .title-hint');
                if (hint) hint.textContent = 'ESC pressed ×' + escCount;
            }
        });
    }

    // Slice D bridge test wiring. game.* is bound by C++ in OnDOMReady, which
    // can race with router's async page injection — retry briefly if missing.
    let bridgeBound = false;
    function bindBridgeTest(attempt) {
        if (bridgeBound) return;
        const g = window.game;
        if (!g || !g.getConfig) {
            if ((attempt || 0) < 20) setTimeout(() => bindBridgeTest((attempt || 0) + 1), 50);
            return;
        }
        bridgeBound = true;

        const verEl  = document.getElementById('bt-version');
        const sens   = document.getElementById('bt-sens');
        const sensVal = document.getElementById('bt-sens-val');

        if (verEl) verEl.textContent = g.getVersion();

        const cur = g.getConfig('input.sensitivity');
        if (sens && sensVal) {
            if (cur !== null && cur !== undefined) {
                sens.value = cur;
                sensVal.textContent = Number(cur).toFixed(4);
            } else {
                sensVal.textContent = 'unset';
            }
            sens.addEventListener('input', () => {
                const v = parseFloat(sens.value);
                sensVal.textContent = v.toFixed(4);
                g.setConfig('input.sensitivity', v);   // realtime: fires C++ subscriber
            });
        }

        document.getElementById('bt-save')
            ?.addEventListener('click', () => g.saveConfig());
        document.getElementById('bt-state')
            ?.addEventListener('click', () => g.setState('hud'));
        document.getElementById('bt-start')
            ?.addEventListener('click', () => g.startLocalGame());
    }

    function onEnter() {
        console.log('[PageTitle] enter');
        bindOnce();
        bindBridgeTest();
    }

    function onExit() {
        console.log('[PageTitle] exit');
    }

    window.PageTitle = { onEnter, onExit };
})();
