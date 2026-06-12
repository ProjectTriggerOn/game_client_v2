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

    function onEnter() {
        console.log('[PageTitle] enter');
        bindOnce();
    }

    function onExit() {
        console.log('[PageTitle] exit');
    }

    window.PageTitle = { onEnter, onExit };
})();
