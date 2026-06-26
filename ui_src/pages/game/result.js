// result.js — self-contained IIFE; exposes only PageResult = { onEnter, onExit }
//
// The result screen is shown when the match ends (C++ flips GameState to RESULT
// when a snapshot reports matchState == ENDED; UIPolicy_Apply maps RESULT →
// 'result' at Modal level, which frees the cursor for the button).
//
// onMatchResult is global (not gated on onEnter) so the C++ push that arrives
// just before the page is shown still populates the (hidden) DOM. The Return
// button only changes scene via the existing game.returnToTitle verb; the next
// game entry re-arms a fresh match (MockServer::ResetSession).

(function () {
    function rosterHtml(arr) {
        return (arr || []).map((p) =>
            '<div class="sb-row' + (p.me ? ' sb-me' : '') + '">' +
            '<span>#' + p.id + '</span><span>' + p.k + ' / ' + p.d + '</span></div>'
        ).join('');
    }

    // C++ → JS: UI::PushMatchResult(json)
    //   { winner: 0|1|2, red: N, blue: N, scoreboard: { red:[...], blue:[...] } }
    window.onMatchResult = function (json) {
        let d;
        try { d = JSON.parse(json); } catch (e) { return; }

        const WIN_TEXT = ['RED WINS', 'BLUE WINS', 'DRAW'];
        const WIN_CLASS = ['w-red', 'w-blue', 'w-draw'];
        const wl = document.querySelector('#page-result .result-winner');
        if (wl) {
            wl.textContent = WIN_TEXT[d.winner] || 'MATCH OVER';
            wl.className = 'result-winner ' + (WIN_CLASS[d.winner] || '');
        }

        const r = document.querySelector('#page-result .result-red');
        if (r) r.textContent = d.red;
        const b = document.querySelector('#page-result .result-blue');
        if (b) b.textContent = d.blue;

        const sb = d.scoreboard || { red: [], blue: [] };
        const rb = document.querySelector('#page-result .sb-red .sb-rows');
        if (rb) rb.innerHTML = rosterHtml(sb.red);
        const bb = document.querySelector('#page-result .sb-blue .sb-rows');
        if (bb) bb.innerHTML = rosterHtml(sb.blue);
    };

    document.getElementById('page-result')?.addEventListener('click', (e) => {
        if (e.target.closest('#result-return')) window.game?.returnToTitle?.();
    });

    window.PageResult = {
        onEnter() { console.log('[PageResult] enter'); },
        onExit()  { console.log('[PageResult] exit'); },
    };
})();
