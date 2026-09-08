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

    // C++ → JS: UI::PushMatchPhase(matchState, seconds)
    //
    // matchState mirrors the MatchState:: values in net_common.h. Only PLAYING
    // is live; the rest are frozen phases the server holds the world in, and
    // `seconds` is whichever clock that phase is running (the countdown's 3..0
    // during COUNTDOWN). ENDED is deliberately not banner-worthy — the result
    // page takes over the screen for it.
    const MATCH_PLAYING = 0, MATCH_WAITING = 2, MATCH_COUNTDOWN = 3;

    window.onMatchPhaseChanged = function (matchState, seconds) {
        const root = document.querySelector('#page-hud .hud-phase');
        if (!root) return;

        const label = root.querySelector('.hud-phase-label');
        const count = root.querySelector('.hud-phase-count');

        if (matchState === MATCH_WAITING) {
            if (label) label.textContent = 'WAITING FOR PLAYERS';
            if (count) count.textContent = '';
            root.classList.remove('hidden');
        } else if (matchState === MATCH_COUNTDOWN) {
            if (label) label.textContent = 'MATCH STARTS IN';
            // Ceil, so a clock of exactly 3.0 reads "3" and the last fractional
            // slice still reads "1" rather than flashing a 0.
            if (count) count.textContent = Math.max(1, Math.ceil(seconds));
            root.classList.remove('hidden');
        } else {
            root.classList.add('hidden');
        }

        // The match timer shares its wire field with the countdown clock, so
        // blank it outside PLAYING rather than showing 0:03 as a match length.
        if (matchState !== MATCH_PLAYING) setText('#page-hud .hud-match-timer', '--:--');
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

    // Damage vignette: C++ pulses GameHUD.onDamage; the CSS transition owns
    // the decay (instant-on via .flash, 0.45s fade after removal).
    // NOTE: lookup happens on every pulse (like the HP/ammo handlers above) —
    // hud.js loads before router.js injects the page markup, so a top-level
    // getElementById captures null and the flash would silently never fire.
    let vignetteTimer = 0;
    window.GameHUD = window.GameHUD || {};
    GameHUD.onDamage = () => {
      const vignette = document.getElementById('damage-vignette');
      if (!vignette) return;
      vignette.classList.add('flash');
      clearTimeout(vignetteTimer);
      vignetteTimer = setTimeout(() => vignette.classList.remove('flash'), 40);
    };

    function onEnter() { console.log('[PageHud] enter'); }
    function onExit()  { console.log('[PageHud] exit'); }

    window.PageHud = { onEnter, onExit };
})();
