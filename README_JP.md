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
- **3D オーディオ** — バックエンド非依存のファサードの下に miniaudio を配置し、4 系統のミックスバス（SFX / UI / Music / Ambient）、優先度つき奪取に対応したボイスプール、TOML の音表を持ちます。ゲーム内の効果音（発砲・リロード・ジャンプ・着地・被弾・死亡・キル確定、プレイヤーごとの足音メトロノーム）は連続するサーバースナップショットの差分からクライアント側で導出しており、プロトコル変更は一切不要でした。ローカルプレイヤー自身の銃声 / ADS 音は予測系から鳴らすため往復遅延の影響を受けません。バス音量は設定画面から即時反映されます。
- **COD 型リコイルと射撃感** — 見た目のパンチ、実挙動のキック、ブルームを `fireCounter` から（乱数なしで）ローカル予測し、同一内容の `recoil_math.h` を持つサーバーの値と突き合わせて補正します。クロスヘアは実際の拡散コーンに追従し、ADS 中はフェードアウト。ヒットマーカーと被弾時のビネットも備えます。
- **着弾エフェクト** — 当たり判定ワールドへのローカルレイキャストから、面に沿った弾痕デカール（固定長リングバッファ）と火花パーティクルを生成します。
- **レッドドットサイト** — 一人称 / 三人称の武器サイトに描画され、大きさは `weapon.reticle_scale` で即時変更できます。
- **HUD のマッチ進行表示** — チームスコア、キルフィード、スコアボード、試合終了時のリザルト画面をすべてスナップショットから導出します。
- **サーバーと共有する `.map` 形式** — `resource/maps/default.map` を読み込み、接続時にサーバーの `MAP_INFO` の当たり判定チェックサムと実際に読み込んだマップを照合します。

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

実行ファイルの隣にある `config/` フォルダの `config/config.toml` を編集してください（詳細なコメント付きのリファレンスは同ファイル内にあります。以下は項目の一覧です）。ゲーム内で変更した設定は `config/user_settings.toml`（自動生成のオーバーレイ）に書き出され、`config.toml` より優先されます（手書きの `config.toml` がゲームによって上書きされることはありません）。

```toml
[network]
mode        = "mock"      # "mock" | "local" | "remote"
server_port = 7777
local_host  = "127.0.0.1"
remote_host = "127.0.0.1"

[client]
# 旧来のウィンドウサイズ。[display].width/height が 0 より大きい場合はそちらが優先されます。
window_width  = 1920
window_height = 1080

[display]
# mode / width / height / monitor_index は設定画面の APPLY で適用され、15 秒以内に
# 確定しなければ自動で元に戻ります（黒画面になるモードでも復帰できます）。
# vsync / fov / max_fps は即時反映です。
mode          = "windowed"   # "windowed" | "borderless" | "exclusive"
width         = 1920         # 0 = ネイティブ / 自動
height        = 1080
aspect_ratio  = "all"        # UI 側のフィルタのみ: "all" | "16:9" | "16:10" | "21:9" | "4:3"
monitor_index = 0            # DXGI 出力インデックス（0 = プライマリ）
vsync         = true
fov           = 90.0         # FPS カメラの垂直 FOV（度）
max_fps       = 0            # フレーム上限、0 = 無制限

[weapon]
reticle_scale = 2.5          # レッドドットの表示倍率（即時反映）

[log]
enabled = true
root    = "logs"

[debug]
# 起動シーン: "game" | "title" | "ui_test"
#   title   — Ultralight のタイトルメニュー（PLAY / SETTINGS / QUIT）
#   game    — そのままゲームプレイへ
#   ui_test — UI 開発用サンドボックス（ゲームプレイ・3D なし、入力はすべて UI へ）
start_scene           = "title"
error_threshold       = 0.5     # 誤差がこの値以下の CORR 行は出力しない
log_every_correction  = false
log_jump_events       = true
log_softmode_state    = true
softmode_sample_ticks = 16

[audio]
# 5 項目とも即時反映（APPLY ボタン不要）。
master  = 0.8
sfx     = 1.0
ui      = 0.8
music   = 0.6
ambient = 0.5
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
├── audio_catalog.toml     # 音表: ファイル・バス・ミックス / 空間化パラメータ
└── user_settings.toml     # 自動生成のオーバーレイ（実行時に作成）
logs/                      # 実行時に作成
├── ultralight.log
└── <timestamp>/*.log
resource/
├── audio/                 # モノラル 16bit 44.1kHz WAV（武器・キャラクター・UI・環境音）
├── maps/                  # マップデータ（default.map）
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
├── dev.html               # ブラウザ用エントリポイント（mock bridge + 開発オーバーレイ）
├── shared.css
├── router.js              # ページの表示切り替え + ライフサイクルフック
├── dev/
│   ├── README.md          # ブラウザでの UI 開発フロー
│   └── mock_bridge.js     # window.game.* のモック（dev.html からのみ読み込み）
├── fonts/                 # Saira, Share Tech Mono
└── pages/
    ├── title.{html,css,js}
    ├── settings.{html,css,js}
    └── game/
        ├── hud.{html,css,js}
        ├── pause.{html,css,js}
        └── result.{html,css,js}   # 試合終了時のリザルト画面
```

**Debug** ビルドでは `ui_src/` を直接読み込み、ファイル監視により再起動なしで編集を反映します（`.css` はその場でスタイルを再適用、`.html`/`.js` はページをリロードして現在のゲームステートに対応するページへ復帰）。**Release** ビルドでは `resource/ui/` にミラーされたコピーを読み込みます。また `dev.html` は C++ ブリッジの代わりを務めるため、UI だけをブラウザ上で完結して開発できます（`ui_src/dev/README.md` を参照）。

## テスト

`Game/tests/` には、エンジンに依存しないロジック（マップ I/O、デカール、パーティクル、レイキャスト、リコイル計算、音表とスナップショット差分によるイベント導出）のスタンドアロンなテストプログラムを置いています。これらは意図的に `TriggerOn.vcxproj` に含めておらず、各ファイルが独自の `main` を持ち、VS 開発者プロンプトから `cl` で直接ビルドします。

```
cl /nologo /std:c++17 /EHsc /W4 /DPARTICLE_TEST_BUILD /I . /I Graphics Game	ests	est_particle.cpp Graphicsparticle.cpp /Fe:_test_particle.exe
_test_particle.exe
```

各テストの正確なコマンドラインは、それぞれのファイル冒頭のコメントに記載しています。

## ディレクトリ構成

```
Audio/          miniaudio バックエンド、音表、スナップショット差分によるイベント導出
Core/           ウィンドウ、Direct3D 初期化、入力、設定、タイマー
Game/           ゲームループ、プレイヤーロジック、当たり判定、ステートマシン、シーン管理、マップ、着弾エフェクト、UI/マウスポリシー
Game/tests/     スタンドアロンな単体テスト（「テスト」を参照）
Graphics/       シェーダー、モデル (ASSIMP)、スプライト、テクスチャ、カメラ、ライティング、パーティクル、デカール、レティクル
Network/        INetwork インターフェース、ENet クライアント、モックサーバー、リモートプレイヤー、共有リコイル計算
UI/             Ultralight 統合: マネージャ、D3D11 合成、JS ブリッジ、入力キュー、ファイルシステム、ホットリロード
Shaders/        HLSL ソースファイル（3D + UI 合成）
ui_src/         HTML/CSS/JS UI 単一ページアプリ（「ゲーム内 UI」を参照）
tools/          オフライン補助ツール: .map コンバータ、FBX アニメーション情報ダンプ、音声変換スクリプト
ThirdParty/     ENet, ASSIMP, miniaudio, toml++, Ultralight
```

## クレジット

このプロジェクトで使用しているサードパーティ素材・ライブラリ:

- **Low Poly Shooter Pack**（Unity Asset Store）— キャラクター/武器モデル、および `resource/audio/` 以下の音声素材
- **Saira** / **Share Tech Mono**（Google Fonts, SIL OFL 1.1）— `ui_src/fonts/` の UI 用書体
- **miniaudio** — オーディオ再生バックエンド（パブリックドメイン / MIT-0）
- **ENet** — 信頼性のある UDP ネットワーキング
- **Assimp** — モデルインポート
- **toml++** — 設定ファイルのパース
- **Ultralight** — HTML/CSS による UI 描画
- **DirectXTK `WICTextureLoader11`**（Microsoft, MIT）— WIC 画像を D3D11 テクスチャとして読み込み（`Graphics/`）
