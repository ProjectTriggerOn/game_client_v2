// router.js — SPA show/hide switching + page lifecycle hooks + fetch-preloading
// of page HTML at startup
//
// Exposes:
//   Router.show(name)         switch to page `name`; overlays stack on top instead
//   Router.closeOverlay(name) close an overlay
//   Router.back()             return to the previous non-overlay page
//   Router.ready              Promise resolved once all page HTML is loaded
//
// Convention: each page owns a <div id="page-XXX"> plus window.PageXxx =
// { onEnter, onExit }. Page markup lives in pages/<name>.html and is fetched
// and injected into the matching div by this file.

(function () {
    const PAGES = ['title', 'hud', 'pause', 'settings'];
    // pause/settings carry a static `.overlay` class in index.html for their dim
    // background; the router treats every page identically (show one, hide rest).

    const cap  = (s) => s[0].toUpperCase() + s.slice(1);
    const hook = (n) => window['Page' + cap(n)];
    const el   = (n) => document.getElementById('page-' + n);

    const Router = {
        current: null,
        ready: null,           // assigned a Promise by init() below

        // Show exactly one page; hide all others. C++ UIPolicy_Apply drives this
        // with a single definitive page per (scene, GameState), so there is no
        // multi-page stacking to manage. Overlay pages (pause/settings) carry a
        // static `.overlay` class for their dim background; the 3D scene still
        // shows through the transparent View underneath.
        show(name) {
            if (!PAGES.includes(name)) {
                console.warn('[Router] unknown page:', name);
                return;
            }
            if (this.current === name) return;

            if (this.current) hook(this.current)?.onExit?.();

            PAGES.forEach((p) => el(p).classList.add('hidden'));
            el(name).classList.remove('hidden');

            this.current = name;
            hook(name)?.onEnter?.();
        },
    };

    // Slice F hot-reload placeholder
    window.reloadCSS = function (path) {
        const links = document.querySelectorAll('link[rel=stylesheet]');
        for (const l of links) {
            if (l.href.endsWith(path)) {
                const clone = l.cloneNode();
                clone.href = l.href.split('?')[0] + '?t=' + Date.now();
                l.parentNode.replaceChild(clone, l);
                return;
            }
        }
    };

    window.Router = Router;

    // Startup: fetch all pages/*.html → inject into their divs → show title by default
    Router.ready = (async function init() {
        try {
            await Promise.all(PAGES.map(async (name) => {
                const res = await fetch('pages/' + name + '.html');
                if (!res.ok) {
                    console.error('[Router] failed to load pages/' + name + '.html', res.status);
                    return;
                }
                el(name).innerHTML = await res.text();
            }));
            console.log('[Router] all pages loaded');
            Router.show('title');
        } catch (e) {
            console.error('[Router] init failed:', e);
        }
    })();
})();
