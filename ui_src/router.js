// router.js —— SPA 显隐切换 + 页面生命周期钩子 + 启动时 fetch 预加载页面 HTML
//
// 暴露：
//   Router.show(name)         切换到 name 页；如果是 overlay 则叠加显示
//   Router.closeOverlay(name) 关闭 overlay
//   Router.back()             返回上一非 overlay 页
//   Router.ready              Promise，所有页面 HTML 加载完后 resolve
//
// 约定每个页面对应 <div id="page-XXX"> 和 window.PageXxx = { onEnter, onExit }
// 每个页面的 markup 单独放在 pages/<name>.html，由本文件 fetch 后注入对应 div。

(function () {
    const PAGES    = ['title', 'hud'];                  // Slice B 仅这两个；后续切片扩
    const OVERLAYS = new Set([]);                       // Slice B 暂无 overlay

    const cap  = (s) => s[0].toUpperCase() + s.slice(1);
    const hook = (n) => window['Page' + cap(n)];
    const el   = (n) => document.getElementById('page-' + n);

    const Router = {
        current: null,
        stack: [],
        ready: null,           // 由下面的 init() 赋值为 Promise

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

    // Slice F 热重载占位
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

    // 启动：fetch 全部 pages/*.html → 注入对应 div → 默认显示 title
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
