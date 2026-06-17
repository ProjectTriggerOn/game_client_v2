// hud.js — self-contained IIFE; exposes only PageHud = { onEnter, onExit }
//
// HUD data is pushed from C++ each gameplay frame via window.on*Changed
// (docs §8.2). These handlers are global (not gated on onEnter) so a push that
// arrives before the page is shown still updates the DOM. They no-op safely if
// the markup hasn't been injected yet.

(function () {
    function setText(selector, text) {
        const el = document.querySelector(selector);
        if (el) el.textContent = text;
    }

    // C++ → JS: UI::PushHealth(current, max)
    window.onHealthChanged = function (current, max) {
        const pct = max > 0 ? (current / max) * 100 : 0;
        const fill = document.querySelector('#page-hud .hud-hp-fill');
        if (fill) fill.style.width = pct + '%';
        setText('#page-hud .hud-hp-text', Math.round(current) + ' / ' + max);
    };

    // C++ → JS: UI::PushAmmo(current, reserve)
    window.onAmmoChanged = function (current, reserve) {
        setText('#page-hud .hud-ammo-current', current);
        setText('#page-hud .hud-ammo-reserve', reserve);
    };

    function onEnter() { console.log('[PageHud] enter'); }
    function onExit()  { console.log('[PageHud] exit'); }

    window.PageHud = { onEnter, onExit };
})();
