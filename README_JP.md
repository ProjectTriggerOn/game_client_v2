<p align="center">
  <a href="./README.md">English</a> | 日本語
</p>

# TriggerOn Client

Direct3D 11 / Win32 ベースのマルチプレイヤー FPS ゲームクライアント。ゲーム内 UI（メニュー・HUD・設定）は HTML/CSS/JS で記述し、[Ultralight](https://ultralig.ht/) で描画しています。

## 主な機能

- **Direct3D 11** による描画（ライティング、アンリット、スケルタルアニメーション対応 HLSL シェーダー）
- **サーバー権威型ネットコード** — クライアント側予測 + ロールバック方式による再シミュレート補正、リモートプレイヤーはスナップショット補間 / 外挿で滑らかに表示
- **最大 10 人**（5v5 のチーム戦）— チームカラーのモデルをサーバーステートから描画
- **3 つの接続モード**: mock（オフライン）、local（LAN）、remote（インターネット）
- **スケルタルアニメーション** — ASSIMP によるモデル読み込み、スナップショット式クロスフェード + 加算ブレンディング対応
- **Ultralight による HTML/CSS/JS UI** — タイトルメニュー、ゲーム内 HUD、ポーズ / 設定オーバーレイを単一ページアプリ（SPA）として 3D シーンの上に合成。設定はエンジンへ即時反映（例: マウス感度）され、自動生成される `user_settings.toml` に保存されて次回起動時も維持されます。Debug ビルドでは `ui_src/` の変更をホットリロードします。

## 動作環境

- **OS**: Windows 10 以降
- **IDE**: Visual Studio 2022（プラットフォームツールセット v143）
- **C++ 規格**: C++17
- **Ultralight SDK 1.4.0**（win-x64）— `ThirdParty/ultralight/` に同梱（`include/`・`lib/`・`resources/`）。4 つのランタイム DLL（`Ultralight.dll`・`UltralightCore.dll`・`WebCore.dll`・`AppCore.dll`）を `ThirdParty/ultralight/bin/` に配置する必要があります（ポストビルドで実行ファイルの隣にコピーされます）。SDK は <https://ultralig.ht/> から取得してください。

## ビルド

**Visual Studio:**

`TriggerOn.sln` を開き、構成を **Release | x64** に設定してビルドを実行。

**コマンドライン:**

```
msbuild TriggerOn.sln /p:Configuration=Release /p:Platform=x64
```

ポストビルドステップでは以下を行います。

- HLSL シェーダーを `.cso` にコンパイル（UI 合成用の `ui_vs` / `ui_ps` を含む）し、`resource/shader/` にコピー
- Ultralight の DLL と `resources/`（ICU データ + CA 証明書）を実行ファイルの隣にコピー
- **Release** のみ: `ui_src/` を `resource/ui/` にミラーリングしてビルド成果物に同梱

**Debug** では `ui_src/` を直接読み込むため（コピーなし）、ホットリロードが有効になります。

## 設定

実行ファイルと同じディレクトリにある `config.toml` を編集してください。ゲーム内で変更した設定は別ファイル `user_settings.toml`（自動生成）に書き出され、`config.toml` より優先されます（手書きの `config.toml` がゲームによって上書きされることはありません）。

```toml
[network]
mode        = "mock"      # "mock" | "local" | "remote"
server_port = 7777
local_host  = "127.0.0.1"
remote_host = "127.0.0.1"

[client]
window_width  = 1920
window_height = 1080

[log]
enabled = true
root    = "logs"

[debug]
# 起動シーン: "game" | "title" | "ui_test"
#   title   — Ultralight のタイトルメニュー（PLAY / SETTINGS / QUIT）
#   game    — そのままゲームプレイへ
#   ui_test — UI 開発用サンドボックス（ゲームプレイ・3D なし、入力はすべて UI へ）
start_scene = "title"
```

### 接続モード

| モード | 説明 | サーバー要否 |
|--------|------|-------------|
| `mock` | インプロセスモックサーバー（ネットワーク通信なし） | 不要 |
| `local` | ENet UDP で `127.0.0.1` に接続 | 要（ローカル） |
| `remote` | ENet UDP で `remote_host` に接続 | 要（リモート） |

## 実行時に必要なファイル

`TriggerOn.exe` と同じディレクトリに以下が必要です。

```
TriggerOn.exe
assimp-vc143-mt.dll
Ultralight.dll  UltralightCore.dll  WebCore.dll  AppCore.dll
config.toml
resources/                 # Ultralight エンジンリソース（ビルドで配置）
├── cacert.pem             # HTTPS ルート証明書
└── icudt67l.dat           # ICU 国際化データ
resource/
├── audio/                 # BGM・効果音 (.wav)
├── model/                 # 3D モデル・アニメーション (.fbx)
├── shader/                # コンパイル済みシェーダー (.cso, ビルド時生成)
├── texture/               # テクスチャ (.png, .jpg)
└── ui/                    # HTML/CSS/JS UI（Release ビルド時に ui_src/ からミラー）
```

> 単数形 / 複数形に注意: `resources/` は Ultralight が要求するエンジンリソース用フォルダ、`resource/` はゲーム自身のアセット（音声・モデル・シェーダー・テクスチャ・UI）です。

## ゲーム内 UI（`ui_src/`）

ゲーム内 UI は `ui_src/` に素の HTML/CSS/JS による単一ページアプリ（SPA）として置かれています（ビルドツール・フレームワークなし）。単一の Ultralight `View` を 3D シーンの上に合成し、`router.js` が常に 1 ページだけを表示します。どのページを表示するかは C++ 側が現在のゲームステートから決定します。

```
ui_src/
├── index.html             # SPA シェル（Ultralight のエントリポイント）
├── shared.css
├── router.js              # ページの表示切り替え + ライフサイクルフック
└── pages/
    ├── title.{html,css,js}
    ├── settings.{html,css,js}
    └── game/
        ├── hud.{html,css,js}
        └── pause.{html,css,js}
```

**Debug** ビルドでは `ui_src/` を直接読み込み、ファイル監視により再起動なしで編集を反映します（`.css` はその場でスタイルを再適用、`.html`/`.js` はページをリロードして現在のゲームステートに対応するページへ復帰）。**Release** ビルドでは `resource/ui/` にミラーされたコピーを読み込みます。

## ディレクトリ構成

```
Core/           ウィンドウ、Direct3D 初期化、入力、設定、タイマー
Game/           ゲームループ、プレイヤーロジック、当たり判定、ステートマシン、シーン管理、UI/マウスポリシー
Graphics/       シェーダー、モデル (ASSIMP)、スプライト、テクスチャ、カメラ、ライティング
Network/        INetwork インターフェース、ENet クライアント、モックサーバー、リモートプレイヤー
UI/             Ultralight 統合: マネージャ、D3D11 合成、JS ブリッジ、入力キュー、ファイルシステム、ホットリロード
Shaders/        HLSL ソースファイル（3D + UI 合成）
ui_src/         HTML/CSS/JS UI 単一ページアプリ（「ゲーム内 UI」を参照）
ThirdParty/     ENet, ASSIMP, toml++, Ultralight
```
