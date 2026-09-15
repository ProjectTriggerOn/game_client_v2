<p align="center">
  English | <a href="./README_JP.md">日本語</a> | <a href="./README_CN.md">中文</a>
</p>

# Browser UI Development

`ui_src/` is a plain single-page app — no build step, no framework — so the whole
in-game UI can be iterated in a browser with the game unbuilt and no server
running. `dev.html` is the browser entry point: it loads a stand-in for the C++
bridge before anything else, so pages that call `game.*` behave the way they do
in the game.

## The three files that differ

| File | Role | Loaded by |
|------|------|-----------|
| `index.html` | production entry point | Ultralight, inside the game |
| `dev.html` | browser entry point — mock bridge, dev overlay | a browser |
| `dev/mock_bridge.js` | stands in for `window.game.*`; also draws the overlay and binds its shortcuts | `dev.html` only |

Everything else is the same code in both: `router.js`, `shared.css`, `flood.js`,
and every page under `pages/`.

## Running it

**VS Code Live Server (recommended)**

1. Install the *Live Server* extension (Ritwick Dey).
2. Right-click `ui_src/dev.html` → **Open with Live Server**.
3. The browser opens `http://127.0.0.1:5500/.../dev.html`, and any edit to an
   `.html` / `.css` / `.js` file reloads it.

**Or any static server**

```bash
cd game_client/ui_src
python -m http.server 8000
# http://127.0.0.1:8000/dev.html
```

### Do not open `dev.html` from the file system

Under `file://`, `fetch('pages/…')` is blocked by CORS, so every page-markup load
in `router.js` fails and the screen stays blank. It has to be served over HTTP.

## Keep `dev.html`'s page divs in sync with `index.html`

`router.js` has a `PAGES` map, and at startup it fetches each page's markup and
assigns it into `#page-<name>`. A name in `PAGES` with no matching div is not a
missing page — `el(name).innerHTML` throws on `null`, the `Promise.all` rejects,
`init()`'s `catch` swallows it, and `Router.show()` never runs. Every page keeps
its `hidden` class and the harness comes up **blank**, with one
`[Router] init failed: TypeError` in the console and nothing else to go on.

This is not hypothetical: `result` joined `PAGES` when the scoring system landed,
`dev.html` was not updated with it, and the browser harness was dead from that
commit until it was noticed. **When you add a page, add its div to both files.**

## What the mock covers

`dev/mock_bridge.js` implements these `game.*` verbs:

- **Navigation** — `setState`, `startLocalGame`, `returnToTitle`, `nextMatch`,
  `quit`, `getBootPage`
- **Config** — `getConfig`, `setConfig`, `saveConfig`, persisted to
  `localStorage` so it survives a reload
- **Display settings** — `getDisplayInfo` (with a fake monitor list),
  `applyDisplaySettings`, `confirmDisplaySettings`, `revertDisplaySettings`,
  including the 15-second auto-revert countdown
- **Misc** — `getPlayerList`, `getVersion`, `log`

## What it does not

| Not mocked | What you see in the browser |
|------------|-----------------------------|
| `resume`, `openSettings`, `backToPause` | the pause menu's RESUME and SETTINGS buttons do nothing — page JS calls them as `window.game?.resume?.()`, so this is a silent no-op, not an error |
| `uiClick` | no UI click sound (there is no audio engine here anyway) |
| `setFloodDebug`, `getFloodStats` | the flood panel is markup to style, nothing more; `flood.js` feature-checks every bridge call |

The **push direction** is thinner still. In the game, C++ calls
`window.onHealthChanged`, `onAmmoChanged`, `onScoresChanged`, `onKillFeed`,
`onMatchTimerChanged`, `onMatchPhaseChanged`, `onScoreboardVisible`,
`onScoreboardData` and `onMatchResult` as the match runs. None of them fire here
— only `onDisplayRevertTick`, which the mock drives from its revert countdown. So
the HUD and the result screen show their static markup. Call them by hand from
the console to check a state:

```js
onHealthChanged(35)
onMatchPhaseChanged(1, 4.2)      // COUNTDOWN with 4.2s left
```

## Dev overlay

Top-right corner: the current page name plus a button per page. `F1` shows the
HUD, `F2` the title — these are the overlay's own keys and exist only here (in
the game, F1 toggles the collision debug view).

## Where the browser differs from the game

| | Browser | Game |
|---|---|---|
| Fonts | whatever the system has | FreeType + the TTFs under `ui_src/fonts/` |
| Text rendering | the browser's engine | WebKit + FreeType |
| JS engine | V8 / SpiderMonkey | JavaScriptCore (Ultralight's own) |
| DPI | handled by the browser | physical pixels; the device scale is set by `UI::Initialize` |
| `console.log` | DevTools | forwarded through `UIViewListener::OnAddConsoleMessage` in `ui_manager.cpp` to `OutputDebugString` with a `[UI:console]` prefix, so it lands in the VS Output window; the engine's own log is `logs/ultralight.log` |

So the rendering will not match exactly. What the browser is good for is layout,
colour, spacing and animation timing — most of the work — and none of that needs
a five-minute game build to check. Final sign-off still happens in the game.

## Do not build on these

- **WebGL** — Ultralight does not provide it.
- **`requestAnimationFrame`** — the game throttles it to `Renderer::Update`; a
  browser does not.
- **`visualViewport`** — behaves differently; avoid depending on it.
- **Real network I/O** — `fetch` and WebSocket have no backend in the game.

Anything beyond plain DOM and CSS is worth confirming in the game before a page
comes to depend on it.
