<p align="center">
  English | <a href="./README_JP.md">日本語</a>
</p>

# TriggerOn Client

Windows game client for TriggerOn — a multiplayer networked FPS built with Direct3D 11 and Win32, with an in-game UI (menus, HUD, settings) authored in HTML/CSS/JS and rendered through [Ultralight](https://ultralig.ht/).

## Features

- **Direct3D 11** rendering with HLSL shaders (lit, unlit, skeletal animation)
- **Server-authoritative netcode** with client-side prediction, rollback reconciliation, and snapshot interpolation/extrapolation for remote players
- **Up to 10 players** (5v5 team-based) with team-colored models rendered from server state
- **Three network modes**: mock (offline), local (LAN), remote (internet)
- **Skeletal animation** via ASSIMP with snapshot-based cross-fade and additive-layer blending
- **HTML/CSS/JS UI via Ultralight** — title menu, in-game HUD, and pause/settings overlays run as a single-page app composited over the 3D scene. Settings apply live to engine state (e.g. mouse sensitivity) and persist across runs via a machine-written `user_settings.toml`; a Debug-only hot-reload re-applies edits to `ui_src/` without restarting the game.
- **3D audio** via a miniaudio backend behind a backend-agnostic facade — four mix buses (SFX/UI/Music/Ambient), per-sound voice pools with priority stealing, and a TOML sound catalog. Gameplay sounds (shots, reloads, jumps, landings, damage, deaths, kill confirms, a per-player footstep metronome) are *derived* client-side by diffing consecutive server snapshots, so the feature required zero protocol changes; the local player's own weapon/ADS audio instead comes off the prediction path so it isn't delayed by a round trip. Bus volumes are live settings.
- **COD-model recoil and weapon feel** — visual punch, real kick, and bloom are predicted locally from `fireCounter` (no RNG) and reconciled against the server, which runs the same mirrored `recoil_math.h`. The crosshair tracks the real spread cone, fades out while ADS, and is backed by hitmarkers and an under-fire damage vignette.
- **Impact FX** — surface-aligned bullet-hole decals (fixed ring buffer) and spark particle bursts, driven by a local raycast against the collision world.
- **Red-dot reticle** drawn on the first- and third-person weapon sights, size tunable live via `weapon.reticle_scale`.
- **Match flow in the HUD** — team score, kill feed, scoreboard, and an end-of-match result screen, all derived from the snapshot.
- **Shared `.map` format** — the client loads `resource/maps/default.map` and, on connect, checks the server's `MAP_INFO` collision checksum against the map it actually loaded.

## Requirements

- **OS**: Windows 10 or later
- **IDE**: Visual Studio 2022 (platform toolset v143)
- **C++ Standard**: C++17
- **Ultralight SDK 1.4.0** (win-x64) — vendored under `ThirdParty/ultralight/` (`include/`, `lib/`, `resources/`). The four runtime DLLs (`Ultralight.dll`, `UltralightCore.dll`, `WebCore.dll`, `AppCore.dll`) must be present in `ThirdParty/ultralight/bin/`; the post-build step copies them next to the executable. Get the SDK from <https://ultralig.ht/>.

## Build

**From Visual Studio:**

Open `TriggerOn.sln` → select **Release | x64** → Build Solution.

**From command line:**

```
msbuild TriggerOn.sln /p:Configuration=Release /p:Platform=x64
```

The post-build step:

- compiles the HLSL shaders to `.cso` (including the UI compositing shaders `ui_vs`/`ui_ps`) and copies them to `resource/shader/`,
- copies the Ultralight DLLs and `assimp-vc143-mt.dll` next to the executable (`$(TargetDir)`) so the build tree is runnable — the shipped zip instead puts them in `bin/`,
- on **Release**, mirrors `ui_src/` into `resource/ui/` and `ThirdParty/ultralight/resources/` into `resource/ultralight/resources/` so both ship with the build. On **Debug** both are read in place from the source tree (no copy), which enables hot reload.

## Configuration

Edit `config/config.toml` (the `config/` folder beside the executable) — it carries the full commented reference; the excerpt below lists the keys, not their documentation. Settings changed in-game are written to `config/user_settings.toml` (machine-written overlay), which takes precedence over `config.toml` and is never hand-edited.

```toml
[network]
mode        = "mock"      # "mock" | "local" | "remote"
server_port = 7777
local_host  = "127.0.0.1"
remote_host = "127.0.0.1"

[client]
# Legacy window size; [display].width/height take precedence when > 0.
window_width  = 1920
window_height = 1080

[display]
# mode/width/height/monitor_index apply through the settings page APPLY button
# and revert automatically after 15s if not confirmed (a black-screen mode
# restores itself). vsync/fov/max_fps apply live.
mode          = "windowed"   # "windowed" | "borderless" | "exclusive"
width         = 1920         # 0 = native/auto
height        = 1080
aspect_ratio  = "all"        # UI filter only: "all" | "16:9" | "16:10" | "21:9" | "4:3"
monitor_index = 0            # DXGI output index (0 = primary)
vsync         = true
fov           = 90.0         # FPS camera vertical FOV, in degrees
max_fps       = 0            # frame cap; 0 = uncapped

[weapon]
reticle_scale = 2.5          # red-dot size multiplier on the authored quad (live)

[log]
enabled = true
root    = "logs"

[debug]
# Boot scene: "game" | "title" | "ui_test"
#   title   — Ultralight title menu (PLAY / SETTINGS / QUIT)
#   game    — drop straight into gameplay
#   ui_test — UI development sandbox: no gameplay, no 3D, all input goes to the UI
start_scene           = "title"
error_threshold       = 0.5     # skip CORR rows with err <= this
log_every_correction  = false
log_jump_events       = true
log_softmode_state    = true
softmode_sample_ticks = 16

[audio]
# All five are live settings (no APPLY button).
master  = 0.8
sfx     = 1.0
ui      = 0.8
music   = 0.6
ambient = 0.5
```

### Network Modes

| Mode | Description | Server required |
|------|-------------|-----------------|
| `mock` | In-process mock server, no real networking | No |
| `local` | ENet UDP to `127.0.0.1` | Yes (local) |
| `remote` | ENet UDP to `remote_host` | Yes (remote) |

## Runtime Files

Unzipped artifact layout:

```
TriggerOn.exe
bin/                       # All DLLs (see note below)
├── Ultralight.dll  UltralightCore.dll  WebCore.dll  AppCore.dll
├── assimp-vc143-mt.dll    # Model/animation loading
└── msvcp140*.dll  vcruntime140*.dll   # VC++ redistributable
config/
├── config.toml            # Shipped defaults (hand-edited)
├── audio_catalog.toml     # Sound table: files, buses, mix/spatialisation parameters
└── user_settings.toml     # Machine-written overlay (created at runtime)
logs/                      # Created at runtime
├── ultralight.log
└── <timestamp>/*.log
resource/
├── audio/                 # Mono 16-bit 44.1kHz WAVs (weapons, character, ui, ambient)
├── maps/                  # Map data (default.map)
├── model/                 # 3D models and animations (.fbx)
├── shader/                # Compiled shaders (.cso, generated by build)
├── texture/               # Textures (.png, .jpg)
├── ui/                    # HTML/CSS/JS UI, mirrored from ui_src/ (Release builds)
└── ultralight/resources/  # Ultralight engine resources: cacert.pem, icudt67l.dat
```

> Ultralight's engine resources live under `resource/ultralight/resources/`, so there is no longer a confusing `resource/` vs `resources/` pair beside the executable.

> Why `bin/` works: Windows resolves an exe's implicit imports before `WinMain` and only from the exe's own directory, so the directly-imported DLLs are delay-loaded and the exe adds `bin\` to its DLL search path at startup.

## In-Game UI (`ui_src/`)

The in-game UI lives in `ui_src/` as a plain HTML/CSS/JS single-page app — no build tooling, no framework. A single Ultralight `View` is composited over the 3D scene; `router.js` shows exactly one page at a time, driven from C++ by the current game state.

```
ui_src/
├── index.html             # SPA shell (Ultralight entry point)
├── dev.html               # Browser entry point: mock bridge + dev overlay
├── shared.css
├── router.js              # show/hide page switching + lifecycle hooks
├── dev/
│   ├── README.md          # Browser-based UI development workflow
│   └── mock_bridge.js     # Mock window.game.* API, loaded only by dev.html
├── fonts/                 # Saira, Share Tech Mono
└── pages/
    ├── title.{html,css,js}
    ├── settings.{html,css,js}
    └── game/
        ├── hud.{html,css,js}
        ├── pause.{html,css,js}
        └── result.{html,css,js}   # End-of-match screen
```

On **Debug** builds the UI is read straight from `ui_src/`, and a file watcher hot-reloads edits without restarting: changing a `.css` re-applies styles in place, while changing `.html`/`.js` reloads the page and returns to the one matching the current game state. **Release** builds load the copy mirrored into `resource/ui/`. The UI can also be iterated entirely in a browser through `dev.html`, which stands in for the C++ bridge — see `ui_src/dev/README.md`.

## Tests

`Game/tests/` holds standalone test programs for the engine-independent logic: map I/O, decals, particles, raycasting, recoil math, and the audio catalog / snapshot-diff event derivation. They are deliberately **not** part of `TriggerOn.vcxproj` — each defines its own `main` and compiles directly with `cl` from a VS developer prompt, e.g.

```
cl /nologo /std:c++17 /EHsc /W4 /DPARTICLE_TEST_BUILD /I . /I Graphics Game	ests	est_particle.cpp Graphicsparticle.cpp /Fe:_test_particle.exe
_test_particle.exe
```

The exact command line for each test is in the comment at the top of its file.

## Project Structure

```
Audio/          miniaudio backend, sound catalog, snapshot-diff event derivation
Core/           Window, Direct3D init, input, config, timing
Game/           Game loop, player logic, collision, state machine, scenes, map, impact FX, UI/mouse policies
Game/tests/     Standalone unit tests (see Tests)
Graphics/       Shaders, models (ASSIMP), sprites, textures, camera, lighting, particles, decals, reticle
Network/        INetwork interface, ENet client, mock server, remote players, shared recoil math
UI/             Ultralight integration: manager, D3D11 compositor, JS bridge, input queue, filesystem, hot reload
Shaders/        HLSL source files (3D + UI compositing)
ui_src/         HTML/CSS/JS UI single-page app (see In-Game UI)
tools/          Offline helpers: .map converter, FBX animation dump, audio conversion script
ThirdParty/     ENet, ASSIMP, miniaudio, toml++, Ultralight
```

## Credits

Third-party assets and libraries used in this project:

- **Low Poly Shooter Pack** (Unity Asset Store) — character/weapon models and the audio set under `resource/audio/`
- **Saira** / **Share Tech Mono** (Google Fonts, SIL OFL 1.1) — UI typefaces under `ui_src/fonts/`
- **miniaudio** — audio playback backend (public domain / MIT-0)
- **ENet** — reliable UDP networking
- **Assimp** — model import
- **toml++** — configuration parsing
- **Ultralight** — HTML/CSS UI rendering
- **DirectXTK `WICTextureLoader11`** (Microsoft, MIT) — WIC image loading into D3D11 textures (`Graphics/`)
