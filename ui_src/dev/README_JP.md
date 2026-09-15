<p align="center">
  <a href="./README.md">English</a> | 日本語 | <a href="./README_CN.md">中文</a>
</p>

# ブラウザでの UI 開発

`ui_src/` はビルドツールもフレームワークも使わない素の単一ページアプリ（SPA）です。
そのため、ゲームをビルドせず、サーバーも立てないまま、ゲーム内 UI 全体をブラウザ上で
作り込めます。ブラウザ側の入口が `dev.html` で、他のどのスクリプトよりも先に C++
ブリッジの代役を読み込むため、`game.*` を呼ぶページもゲーム内と同じように動きます。

## ゲーム内と違うのはこの 3 ファイルだけ

| ファイル | 役割 | 読み込むのは |
|---------|------|-------------|
| `index.html` | 製品版の入口 | ゲーム内の Ultralight |
| `dev.html` | ブラウザ用の入口。モックブリッジと開発用オーバーレイを追加 | ブラウザ |
| `dev/mock_bridge.js` | `window.game.*` の代役。オーバーレイの描画とショートカットも担当 | `dev.html` のみ |

これ以外は両方で同じコードです（`router.js`、`shared.css`、`flood.js`、
`pages/` 配下の全ページ）。

## 起動方法

**VS Code Live Server（推奨）**

1. 拡張機能 *Live Server*（Ritwick Dey）をインストール
2. `ui_src/dev.html` を右クリック →  **Open with Live Server**
3. `http://127.0.0.1:5500/.../dev.html` がブラウザで開き、`.html` / `.css` / `.js`
   を保存するたびに自動で再読み込みされます

**または任意の静的サーバー**

```bash
cd game_client/ui_src
python -m http.server 8000
# http://127.0.0.1:8000/dev.html
```

### `dev.html` をファイルから直接開かないこと

`file://` では `fetch('pages/…')` が CORS で拒否されるため、`router.js` のページ
読み込みがすべて失敗し、画面が真っ白になります。必ず HTTP 経由で開いてください。

## `dev.html` のページ div は `index.html` と揃えておくこと

`router.js` は `PAGES` テーブルを持ち、起動時に各ページのマークアップを取得して
`#page-<名前>` に流し込みます。`PAGES` にあるのに対応する div がないと、
「そのページだけ表示されない」では済みません。`el(name).innerHTML` が `null` に
対する代入で例外を投げ、`Promise.all` が reject し、`init()` の `catch` がそれを
握り潰し、`Router.show()` が一度も呼ばれません。結果として全ページが `hidden` の
まま残り、コンソールに `[Router] init failed: TypeError` の 1 行だけを残して
**画面は真っ白**になります。

これは想定の話ではありません。スコア機能が入ったときに `result` が `PAGES` へ
追加された一方で `dev.html` は更新されず、それ以降ブラウザ側の開発環境は気づかれる
まで死んでいました。**ページを追加したら、必ず両方のファイルに div を足してください。**

## モックが実装しているもの

`dev/mock_bridge.js` が用意している `game.*`:

- **画面遷移** — `setState` / `startLocalGame` / `returnToTitle` / `nextMatch` /
  `quit` / `getBootPage`
- **設定** — `getConfig` / `setConfig` / `saveConfig`。`localStorage` に保存されるので
  リロードしても残ります
- **ディスプレイ設定** — `getDisplayInfo`（モニター一覧はダミー）、
  `applyDisplaySettings` / `confirmDisplaySettings` / `revertDisplaySettings`。
  15 秒の自動復帰カウントダウンも含みます
- **その他** — `getPlayerList` / `getVersion` / `log`

## 実装していないもの

| 未実装 | ブラウザでの見え方 |
|--------|------------------|
| `resume` / `openSettings` / `backToPause` | ポーズ画面の RESUME・SETTINGS を押しても何も起きません。ページ側は `window.game?.resume?.()` の形で呼ぶので、エラーにはならず黙って何もしない状態になります |
| `uiClick` | UI のクリック音が鳴りません（そもそもここにオーディオエンジンはありません） |
| `setFloodDebug` / `getFloodStats` | フラッドパネルは見た目を調整するためのマークアップだけです。`flood.js` はブリッジ呼び出しを毎回チェックしてから使います |

**C++ → JS の通知**はさらに手薄です。ゲーム内では試合の進行に合わせて C++ が
`window.onHealthChanged` / `onAmmoChanged` / `onScoresChanged` / `onKillFeed` /
`onMatchTimerChanged` / `onMatchPhaseChanged` / `onScoreboardVisible` /
`onScoreboardData` / `onMatchResult` を呼びますが、ここではどれも発火しません。
唯一 `onDisplayRevertTick` だけがモックの復帰カウントダウンから呼ばれます。つまり
HUD とリザルト画面は静的なマークアップのまま表示されます。状態を確認したいときは
コンソールから直接呼んでください。

```js
onHealthChanged(35)
onMatchPhaseChanged(1, 4.2)      // COUNTDOWN 残り 4.2 秒
```

## 開発用オーバーレイ

右上に現在のページ名とページ切り替えボタンが出ます。`F1` で HUD、`F2` でタイトルに
移動します。これはオーバーレイ独自のキーで、ここにしかありません（ゲーム内の F1 は
当たり判定のデバッグ表示の切り替えです）。

## ブラウザとゲームの違い

| | ブラウザ | ゲーム |
|---|---------|--------|
| フォント | OS にあるもの | FreeType + `ui_src/fonts/` の TTF |
| 文字の描画 | ブラウザのエンジン | WebKit + FreeType |
| JS エンジン | V8 / SpiderMonkey | JavaScriptCore（Ultralight 同梱） |
| DPI | ブラウザが処理 | 物理ピクセル。デバイススケールは `UI::Initialize` が決定 |
| `console.log` | DevTools | `ui_manager.cpp` の `UIViewListener::OnAddConsoleMessage` が `[UI:console]` を付けて `OutputDebugString` へ転送するため VS の出力ウィンドウに出ます。エンジン自身のログは `logs/ultralight.log` |

したがって描画結果が完全に一致することはありません。ブラウザで見られるのはレイアウト、
配色、余白、アニメーションのタイミング — 作業量としては大半 — で、そこにゲームの
ビルドを待つ理由はありません。最終確認だけはゲーム上で行ってください。

## これらに依存しないこと

- **WebGL** — Ultralight は提供していません。
- **`requestAnimationFrame`** — ゲーム内では `Renderer::Update` に合わせて間引かれます。
  ブラウザにはその制限がありません。
- **`visualViewport`** — 挙動が異なるため、依存しないでください。
- **実際の通信** — ゲーム内の `fetch` / WebSocket には応答するバックエンドがありません。

素の DOM と CSS を超えるものは、ページが依存してしまう前にゲーム上で確認する価値が
あります。
