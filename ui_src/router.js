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
    const PAGES    = ['title', 'hud'];                  // Slice B: these two only; later slices extend
    const OVERLAYS = new Set([]);                       // Slice B: no overlays yet

    const cap  = (s) => s[0].toUpperCase() + s.slice(1);
    const hook = (n) => window['Page' + cap(n)];
    const el   = (n) => document.getElementById('page-' + n);

    const Router = {
        current: null,
        stack: [],
        ready: null,           // assigned a Promise by init() below

        show(name) {
            if (!PAGES.includes(name)) {
                console.warn('[Router] unknown page:', name);
                return;
            }
            if (this.current === name) return;

            if (this.current) hook(this.current)?.onExit?.();

            if (!OVERLAYS.has(name)) {
                PAGES.forEach((p) => {
                    if (!OVERLAYS.has(p)) el(p).classList.add('hidden');
                });
            }
            el(name).classList.remove('hidden');

            if (this.current && !OVERLAYS.has(this.current)) {
                this.stack.push(this.current);
            }
            this.current = name;
            hook(name)?.onEnter?.();
        },

        closeOverlay(name) {
            if (!OVERLAYS.has(name)) return;
            el(name).classList.add('hidden');
            hook(name)?.onExit?.();
            if (this.current === name) {
                this.current = this.stack[this.stack.length - 1] || null;
            }
        },

        back() {
            const prev = this.stack.pop();
            if (prev) this.show(prev);
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
