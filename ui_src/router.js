// router.js — SPA show/hide switching + page lifecycle hooks + fetch-preloading
//
// Two switching modes (see docs §5.6 + the native-vs-Ultralight design notes):
//   - State-driven (game scene): C++ UIPolicy_Apply calls Router.show(name)
//     directly; the page is a pure function of GameState, no nav stack.
//   - Navigation-driven (menus/lobby): JS uses Router.navigate(name) / back()
//     for free user navigation between sibling pages, with a small history stack
//     so a shared page (e.g. settings) can return to wherever it was opened from.
//
// PAGES maps a logical page NAME → its markup PATH under pages/. The logical name
// is what everything else uses (the <div id="page-NAME">, window.PageName hooks,
// Router.show(name), C++ ShowPage(name)); only the file location lives in PAGES,
// so pages can be foldered by scene without touching call sites.

(function () {
    const PAGES = {
        'title':    'title',        // top-level / title scene
        'settings': 'settings',     // shared (game pause + future lobby)
        'hud':      'game/hud',     // game scene
        'pause':    'game/pause',   // game scene
        'result':   'game/result',  // game scene end screen (RESULT GameState)
        'editor':   'editor',       // level editor viewport overlay (M2 stub, M4 panels)
        // future: 'lobby-home': 'lobby/home', 'armory': 'lobby/armory', ...
    };

    const names = () => Object.keys(PAGES);
    const cap   = (s) => s.split('-').map(w => w[0].toUpperCase() + w.slice(1)).join('');
    const hook  = (n) => window['Page' + cap(n)];   // 'lobby-home' → window.PageLobbyHome
    const el    = (n) => document.getElementById('page-' + n);

    const Router = {
        current: null,
        stack: [],              // navigation history (navigation-driven scenes only)
        ready: null,            // Promise resolved once all page markup is loaded

        // Low-level switch: show exactly one page, hide the rest. Does NOT touch
        // the nav stack. C++ (state-driven) and the boot/reload path use this.
        show(name) {
            if (!(name in PAGES)) {
                console.warn('[Router] unknown page:', name);
                return;
            }
            if (this.current === name) return;

            if (this.current) hook(this.current)?.onExit?.();

            names().forEach((p) => el(p).classList.add('hidden'));
            el(name).classList.remove('hidden');

            this.current = name;
            hook(name)?.onEnter?.();
        },

        // Navigation-driven: remember where we are, then go to `name`.
        // Use this from menu/lobby buttons so back() can return here.
        navigate(name) {
            if (this.current && this.current !== name) this.stack.push(this.current);
            this.show(name);
        },

        // Pop the nav history and return there. No-op if the stack is empty.
        back() {
            const prev = this.stack.pop();
            if (prev) this.show(prev);
        },

        // Clear nav history (call when a scene takes over via state-driven control).
        resetNav() {
            this.stack.length = 0;
        },
    };

    // Hot-reload helper (Slice F): re-insert a stylesheet <link> with a cache-bust
    // query so an edited CSS file is re-fetched without a full page reload.
    window.reloadCSS = function (path) {
        const links = document.querySelectorAll('link[rel=stylesheet]');
        for (const l of links) {
            // Compare against the query-stripped href: after the first reload the
            // link already carries a ?t=… cache-bust, so matching the raw href
            // would fail every time after the first (hot reload "only works once").
            if (l.href.split('?')[0].endsWith(path)) {
                const clone = l.cloneNode();
                clone.href = l.href.split('?')[0] + '?t=' + Date.now();
                l.parentNode.replaceChild(clone, l);
                return;
            }
        }
    };

    window.Router = Router;

    // Loading curtain — a top-most black overlay (NOT a Router page, so it
    // persists across the page/scene switch happening underneath it). Toggled by
    // C++ (UI::SetCurtain) to mask the scene-transition init hitch; the opacity
    // fade is pure CSS (#curtain in shared.css). The #curtain element is static
    // markup in index.html, so it exists from DOMReady — no fetch/inject race.
    window.Curtain = {
        show() { document.getElementById('curtain')?.classList.add('visible'); },
        hide() { document.getElementById('curtain')?.classList.remove('visible'); },
    };

    // Startup: fetch every page's markup → inject into its div → show the boot
    // page. The boot page comes from C++ (game.getBootPage) so a hot-reload
    // View::Reload lands back on the page matching the current GameState instead
    // of always 'title' (docs §10). Falls back to 'title' in the browser dev
    // harness or before the bridge is bound.
    Router.ready = (async function init() {
        try {
            await Promise.all(names().map(async (name) => {
                const path = 'pages/' + PAGES[name] + '.html';
                const res = await fetch(path);
                if (!res.ok) {
                    console.error('[Router] failed to load ' + path, res.status);
                    return;
                }
                el(name).innerHTML = await res.text();
            }));
            console.log('[Router] all pages loaded');
            Router.show(window.game?.getBootPage?.() || 'title');
        } catch (e) {
            console.error('[Router] init failed:', e);
        }
    })();
})();
