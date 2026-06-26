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

    const teamName = (t) => (t === 0 ? 'RED' : 'BLUE');
    const teamClass = (t) => (t === 0 ? 'red' : 'blue');

    // C++ → JS: UI::PushScores(red, blue)
    window.onScoresChanged = function (red, blue) {
        setText('#page-hud .hud-score-red', red);
        setText('#page-hud .hud-score-blue', blue);
    };

    // C++ → JS: UI::PushMatchTimer(secondsRemaining) — format M:SS
    window.onMatchTimerChanged = function (secs) {
        secs = Math.max(0, Math.floor(secs));
        const m = Math.floor(secs / 60);
        const s = secs % 60;
        setText('#page-hud .hud-match-timer', m + ':' + (s < 10 ? '0' + s : s));
    };

    // C++ → JS: UI::PushKillFeed(killerId, victimId, killerTeam, victimTeam)
    window.onKillFeed = function (killerId, victimId, killerTeam, victimTeam) {
        const feed = document.querySelector('#page-hud .hud-killfeed');
        if (!feed) return;
        const row = document.createElement('div');
        row.className = 'kf-row';
        row.innerHTML =
            '<span class="kf-' + teamClass(killerTeam) + '">' + teamName(killerTeam) + ' #' + killerId + '</span>' +
            ' <span class="kf-arrow">&rarr;</span> ' +
            '<span class="kf-' + teamClass(victimTeam) + '">' + teamName(victimTeam) + ' #' + victimId + '</span>';
        feed.prepend(row);
        while (feed.childNodes.length > 5) feed.removeChild(feed.lastChild);
        setTimeout(() => row.remove(), 5000);
    };

    function renderRoster(selector, arr) {
        const box = document.querySelector(selector);
        if (!box) return;
        box.innerHTML = (arr || []).map((p) =>
            '<div class="sb-row' + (p.me ? ' sb-me' : '') + '">' +
            '<span>#' + p.id + '</span><span>' + p.k + ' / ' + p.d + '</span></div>'
        ).join('');
    }

    // C++ → JS: UI::PushScoreboard(json) — { red: [...], blue: [...] }
    window.onScoreboardData = function (json) {
        let d;
        try { d = JSON.parse(json); } catch (e) { return; }
        renderRoster('#page-hud .sb-red .sb-rows', d.red);
        renderRoster('#page-hud .sb-blue .sb-rows', d.blue);
    };

    // C++ → JS: UI::PushScoreboardVisible(visible)
    window.onScoreboardVisible = function (v) {
        const sb = document.querySelector('#page-hud .hud-scoreboard');
        if (sb) sb.classList.toggle('hidden', !v);
    };

    function onEnter() { console.log('[PageHud] enter'); }
    function onExit()  { console.log('[PageHud] exit'); }

    window.PageHud = { onEnter, onExit };
})();
