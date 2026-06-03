// hud.js —— IIFE 自封装，对外只暴露 PageHud = { onEnter, onExit }

(function () {
    function onEnter() {
        console.log('[PageHud] enter');
    }

    function onExit() {
        console.log('[PageHud] exit');
    }

    window.PageHud = { onEnter, onExit };
})();
