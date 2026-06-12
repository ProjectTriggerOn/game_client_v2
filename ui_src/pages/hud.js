// hud.js — self-contained IIFE; exposes only PageHud = { onEnter, onExit }

(function () {
    function onEnter() {
        console.log('[PageHud] enter');
    }

    function onExit() {
        console.log('[PageHud] exit');
    }

    window.PageHud = { onEnter, onExit };
})();
