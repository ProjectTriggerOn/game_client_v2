<p align="center">
  <a href="./README.md">English</a> | 日本語
</p>

# TriggerOn Client

Direct3D 11 / Win32 ベースのマルチプレイヤー FPS ゲームクライアント。

## 主な機能

- **Direct3D 11** による描画（ライティング、アンリット、スケルタルアニメーション対応 HLSL シェーダー）
- **サーバー権威型ネットコード** — クライアント側予測とサーバー補正のスムーズな補間
- **3 つの接続モード**: mock（オフライン）、local（LAN）、remote（インターネット）
- **スケルタルアニメーション** — ASSIMP によるモデル読み込み、クロスフェードブレンド対応

## 動作環境

- **OS**: Windows 10 以降
- **IDE**: Visual Studio 2022（プラットフォームツールセット v143）
- **C++ 規格**: C++17

## ビルド

**Visual Studio:**

`TriggerOn.sln` を開き、構成を **Release | x64** に設定してビルドを実行。

**コマンドライン:**

```
msbuild TriggerOn.vcxproj /p:Configuration=Release /p:Platform=x64
```

ビルド時に HLSL シェーダーが `.cso` にコンパイルされ、ポストビルドステップで `resource/shader/` にコピーされます。

## 設定

実行ファイルと同じディレクトリにある `config.toml` を編集してください。

```toml
[network]
mode = "mock"             # "mock" | "local" | "remote"
server_port = 7777
local_host  = "127.0.0.1"
remote_host = "127.0.0.1"

[client]
window_width  = 1280
window_height = 720
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
config.toml
resource/
├── audio/       # BGM・効果音 (.wav)
├── model/       # 3D モデル・アニメーション (.fbx)
├── shader/      # コンパイル済みシェーダー (.cso, ビルド時生成)
└── texture/     # テクスチャ (.png, .jpg)
```

## ディレクトリ構成

```
Core/           ウィンドウ、Direct3D 初期化、入力、設定、タイマー
Game/           ゲームループ、プレイヤーロジック、当たり判定、ステートマシン、シーン管理
Graphics/       シェーダー、モデル (ASSIMP)、スプライト、テクスチャ、カメラ、ライティング
Network/        INetwork インターフェース、ENet クライアント、モックサーバー、リモートプレイヤー
Shaders/        HLSL ソースファイル
ThirdParty/     ENet, ASSIMP, toml++
```
