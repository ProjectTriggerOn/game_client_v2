// Flood debug panel (Debug builds only). Drives game.setFloodDebug and polls
// game.getFloodStats for the readout. Shown/hidden from C++ via
// window.FloodPanel.show()/hide() (F8). Inert if the bridge API is absent
// (e.g. a Release build) — every bridge call is feature-checked.
(function () {
    var panel = null, statsTimer = null;

    function el(id) { return document.getElementById(id); }

    // Push the current control values to the C++ flood driver.
    function apply() {
        if (!window.game || !game.setFloodDebug) return;
        var on = el('flood-on').checked;
        var rate = parseInt(el('flood-rate').value, 10) || 0;
        var mode = parseInt(el('flood-mode').value, 10) || 0;
        el('flood-rate-val').textContent = rate;
        game.setFloodDebug(on, rate, mode);
        poll();
    }

    // Pull live stats (actual send rate + this client's snapshot interval).
    function poll() {
        if (!window.game || !game.getFloodStats) return;
        try {
            var s = JSON.parse(game.getFloodStats());
            el('flood-readout').textContent =
                'send ' + s.sendRate + '/s   snap ' + s.snapMs + 'ms';
        } catch (e) {}
    }

    function wire() {
        panel = el('flood-panel');
        if (!panel) return;
        el('flood-on').addEventListener('change', apply);
        el('flood-rate').addEventListener('input', apply);
        el('flood-mode').addEventListener('change', apply);
    }

    window.FloodPanel = {
        show: function () {
            if (!panel) wire();
            if (panel) panel.classList.remove('hidden');
            poll();
            if (!statsTimer) statsTimer = setInterval(poll, 250);
        },
        hide: function () {
            if (panel) panel.classList.add('hidden');
            if (statsTimer) { clearInterval(statsTimer); statsTimer = null; }
        }
    };
})();
