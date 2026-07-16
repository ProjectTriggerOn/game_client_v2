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
- Ultralight の DLL と `assimp-vc143-mt.dll` を実行ファイルの隣（`$(TargetDir)`）にコピーし、ビルドツリーのまま起動できるようにする（配布 zip ではこれらは `bin/` に配置されます）
- **Release** のみ: `ui_src/` を `resource/ui/` に、`ThirdParty/ultralight/resources/` を `resource/ultralight/resources/` にミラーリングしてビルド成果物に同梱

**Debug** ではどちらもソースツリーから直接読み込むため（コピーなし）、ホットリロードが有効になります。

## 設定

実行ファイルの隣にある `config/` フォルダの `config/config.toml` を編集してください。ゲーム内で変更した設定は `config/user_settings.toml`（自動生成のオーバーレイ）に書き出され、`config.toml` より優先されます（手書きの `config.toml` がゲームによって上書きされることはありません）。

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

配布 zip を展開した構成は以下のとおりです。

```
TriggerOn.exe
bin/                       # すべての DLL（下記の注記を参照）
├── Ultralight.dll  UltralightCore.dll  WebCore.dll  AppCore.dll
├── assimp-vc143-mt.dll    # モデル・アニメーション読み込み
└── msvcp140*.dll  vcruntime140*.dll   # VC++ 再頒布可能パッケージ
config/
├── config.toml            # 同梱のデフォルト設定（手書き用）
└── user_settings.toml     # 自動生成のオーバーレイ（実行時に作成）
logs/                      # 実行時に作成
├── ultralight.log
└── <timestamp>/*.log
resource/
├── maps/                  # マップデータ
├── model/                 # 3D モデル・アニメーション (.fbx)
├── shader/                # コンパイル済みシェーダー (.cso, ビルド時生成)
├── texture/               # テクスチャ (.png, .jpg)
├── ui/                    # HTML/CSS/JS UI（Release ビルド時に ui_src/ からミラー）
└── ultralight/resources/  # Ultralight エンジンリソース: cacert.pem, icudt67l.dat
```

> Ultralight のエンジンリソースは `resource/ultralight/resources/` 配下に置かれるため、実行ファイルの隣に `resource/` と `resources/` が並ぶ紛らわしい構成はなくなりました。

> `bin/` が機能する理由: Windows は exe の暗黙的インポートを `WinMain` より前に、しかも exe 自身のディレクトリからのみ解決します。そのため直接インポートしている DLL は遅延読み込みにし、exe は起動時に `bin\` を DLL 検索パスへ追加しています。

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
