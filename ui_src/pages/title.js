// title.js —— IIFE 自封装，对外只暴露 PageTitle = { onEnter, onExit }

(function () {
    function onEnter() {
        console.log('[PageTitle] enter');
    }

    function onExit() {
        console.log('[PageTitle] exit');
    }

    window.PageTitle = { onEnter, onExit };
})();
