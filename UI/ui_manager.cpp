#include "ui_manager.h"
#include "ui_d3d11_renderer.h"
#include "ui_filesystem.h"
#include "ui_input_queue.h"
#include "ui_bridge.h"
#include "ui_hot_reload.h"

#include <Ultralight/Ultralight.h>
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/Renderer.h>
#include <Ultralight/View.h>
#include <Ultralight/Bitmap.h>
#include <Ultralight/platform/Surface.h>
#include <Ultralight/KeyEvent.h>
#include <Ultralight/MouseEvent.h>
#include <Ultralight/ScrollEvent.h>
#include <Ultralight/Listener.h>
#include <Ultralight/ConsoleMessage.h>
#include <AppCore/Platform.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <memory>
#include <vector>
#include <array>

namespace {

ultralight::RefPtr<ultralight::Renderer> g_renderer;
ultralight::RefPtr<ultralight::View>     g_view;
UI::D3D11BitmapRenderer                  g_d3d_renderer;
std::unique_ptr<UI::UIFileSystem>        g_filesystem;  // must outlive the View; Ultralight keeps a raw pointer
int g_width = 0, g_height = 0;

// Monitor DPI zoom (dpi/96). The effective device scale is derived from this and
// the current surface width so the CSS viewport stays >= ~1280 (see DeviceScaleFor).
float g_dpiScale = 1.0f;
// The device scale currently applied to the View. Input event coordinates must be
// divided by this: Ultralight's MouseEvent x/y are PAGE units, but WndProc taps
// client (screen) pixels, and device_scale maps page units → screen pixels
// (View.h). At scale 1.0 they're equal; above it they diverge and hover/clicks
// land off by the scale factor.
float g_deviceScale = 1.0f;
float DeviceScaleFor(int w) {
    float s = g_dpiScale;
    const float capByWidth = (w > 0) ? (float)w / 1280.0f : s;  // keep CSS width >= 1280
    if (s > capByWidth) s = capByWidth;
    if (s < 1.0f) s = 1.0f;
    return s;
}

// Slice C: single global value; Slice E will drive this from OnGameStateChanged.
// Defaults to Interactive — we boot into the title page, which is interactive.
UI::InteractiveLevel g_interactiveLevel = UI::InteractiveLevel::Interactive;

// JS console.log / engine warnings → VS Output window ("[UI:console]" prefix).
// Without this, console output from pages is completely invisible
// (the Logger interface only covers engine logs).
class UIViewListener : public ultralight::ViewListener {
public:
    void OnAddConsoleMessage(ultralight::View*,
                             const ultralight::ConsoleMessage& msg) override {
        std::string line = "[UI:console] ";
        line += msg.message().utf8().data();
        char loc[64];
        wsprintfA(loc, "  (line %u)\n", msg.line_number());
        line += loc;
        OutputDebugStringA(line.c_str());
    }
};
UIViewListener g_viewListener;

// The JSContext is reset on every navigation/reload — rebind game.* each time
// the DOM is ready (docs §8.3).
class UILoadListener : public ultralight::LoadListener {
public:
    void OnDOMReady(ultralight::View* caller, uint64_t /*frame_id*/,
                    bool is_main_frame, const ultralight::String& /*url*/) override {
        if (!is_main_frame) return;
        UI::Bridge::Register(caller);
    }
};
UILoadListener g_loadListener;

// Page switch requested from inside a JS callback; applied at the start of the
// next Render to avoid re-entrant script evaluation.
std::string g_deferredPage;

// Pending HUD values. Push* (called from gameplay code, mid-frame) only stores;
// the actual C++→JS call is flushed inside Render. Reason: calling JS via
// LockJSContext/SetJSContext from outside Render (or a JSC callback) leaves the
// global JS context dangling once the lock releases — the call silently fails
// AND corrupts later JS interaction (observed: HUD never updated + crash when
// opening settings). Render is the one proven-safe JS-call point per frame.
struct PendingHud {
    bool healthDirty = false; int health = 0, healthMax = 0;
    bool ammoDirty   = false; int ammo = 0, ammoReserve = 0;
    bool scoresDirty = false; int red = 0, blue = 0;
    bool timerDirty  = false; float matchTime = 0.0f;
    bool sbVisDirty  = false; bool sbVisible = false;
    bool sbDataDirty = false; std::string sbJson;
    bool resultDirty = false; std::string resultJson;
    bool revertDirty = false; int  revertSec  = 0;
};
PendingHud g_pendingHud;
// Kill-feed events are discrete, so they queue (not a dirty flag): every kill
// pushed in a frame is replayed at flush. {killerId, victimId, killerTeam, victimTeam}.
std::vector<std::array<int, 4>> g_pendingKills;

std::string GetExeDirA() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(buf);
    return s.substr(0, s.find_last_of("\\/")) + "\\";
}

// Debug:   exe is in game_client/x64/Debug/, ui_src is in game_client/ → two levels up
// Release: ui_src has been mirrored to resource/ui/ next to the exe
std::string GetUiRoot() {
#if defined(_DEBUG) || defined(DEBUG)
    return GetExeDirA() + "..\\..\\ui_src";
#else
    return GetExeDirA() + "resource\\ui";
#endif
}

}  // namespace

namespace UI {

void Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int w, int h, float dpiScale) {
    g_width  = w;
    g_height = h;
    g_dpiScale = (dpiScale > 0.0f) ? dpiScale : 1.0f;

    // 1. Config — resource_path_prefix must be exe-relative, otherwise we crash
    //    whenever the CWD is wrong (see docs/ultralight_integration.md §3.5)
    const std::string resPath = GetExeDirA() + "resources\\";
    ultralight::Config config;
    config.resource_path_prefix = resPath.c_str();
    ultralight::Platform::instance().set_config(config);

    // 2. FileSystem — our own implementation; root dir switches on Debug/Release
    g_filesystem = std::make_unique<UI::UIFileSystem>(GetUiRoot());
    ultralight::Platform::instance().set_file_system(g_filesystem.get());

    // 3. FontLoader (required — fonts won't render without it)
    ultralight::Platform::instance().set_font_loader(
        ultralight::GetPlatformFontLoader()
    );

    // 4. Logger
    ultralight::Platform::instance().set_logger(
        ultralight::GetDefaultLogger("ultralight.log")
    );

    // 5. Renderer + View
    g_renderer = ultralight::Renderer::Create();
    if (!g_renderer) {
        OutputDebugStringA("[UI] Renderer::Create failed\n");
        return;
    }

    ultralight::ViewConfig vc;
    vc.is_accelerated = false;        // CPU / BitmapSurface path
    vc.is_transparent = true;         // unpainted HTML regions reveal the 3D scene underneath
    g_deviceScale = DeviceScaleFor(w);
    vc.initial_device_scale = (double)g_deviceScale;  // DPI zoom, clamped by width
    g_view = g_renderer->CreateView((uint32_t)w, (uint32_t)h, vc, nullptr);
    if (!g_view) {
        OutputDebugStringA("[UI] CreateView failed\n");
        return;
    }

    g_view->set_view_listener(&g_viewListener);   // console.log → VS Output window
    g_view->set_load_listener(&g_loadListener);   // OnDOMReady → bind game.* (Bridge)

    g_view->LoadURL("file:///index.html");

    // 6. D3D11 renderer
    if (!g_d3d_renderer.Initialize(dev, ctx, w, h)) {
        OutputDebugStringA("[UI] D3D11BitmapRenderer::Initialize failed\n");
    }

    // 7. Hot reload (Debug only; no-op stub in Release). Watch the same dir the
    //    FileSystem reads, so edits to the live source are picked up (docs §10).
#if defined(_DEBUG) || defined(DEBUG)
    HotReload::Start(GetUiRoot().c_str());
#endif
}

void Resize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (w == g_width && h == g_height) return;
    // View::Resize keeps the page, JS context and listeners; Ultralight marks the
    // whole surface dirty, so the next Render re-uploads a full-size frame.
    if (g_view) {
        g_view->Resize((uint32_t)w, (uint32_t)h);
        g_deviceScale = DeviceScaleFor(w);                    // larger surface → more scale
        g_view->set_device_scale((double)g_deviceScale);
    }
    g_d3d_renderer.Resize(w, h);
    g_width  = w;
    g_height = h;
}

void Render() {
    if (!g_renderer || !g_view) return;

    // Hot reload: drain watched-file changes on the main thread before any other
    // JS interaction this frame (Debug only; no-op stub in Release).
#if defined(_DEBUG) || defined(DEBUG)
    HotReload::Poll();
#endif

    // Apply a page switch requested from a JS callback (game.setState)
    if (!g_deferredPage.empty()) {
        std::string page;
        page.swap(g_deferredPage);
        ShowPage(page.c_str());
    }

    // Flush pending HUD pushes here (the safe JS-call point), not from gameplay code
    if (g_pendingHud.healthDirty) {
        UI::Bridge::PushHealth(g_pendingHud.health, g_pendingHud.healthMax);
        g_pendingHud.healthDirty = false;
    }
    if (g_pendingHud.ammoDirty) {
        UI::Bridge::PushAmmo(g_pendingHud.ammo, g_pendingHud.ammoReserve);
        g_pendingHud.ammoDirty = false;
    }
    if (g_pendingHud.scoresDirty) {
        UI::Bridge::PushScores(g_pendingHud.red, g_pendingHud.blue);
        g_pendingHud.scoresDirty = false;
    }
    if (g_pendingHud.timerDirty) {
        UI::Bridge::PushMatchTimer(g_pendingHud.matchTime);
        g_pendingHud.timerDirty = false;
    }
    if (g_pendingHud.sbVisDirty) {
        UI::Bridge::PushScoreboardVisible(g_pendingHud.sbVisible);
        g_pendingHud.sbVisDirty = false;
    }
    if (g_pendingHud.sbDataDirty) {
        UI::Bridge::PushScoreboard(g_pendingHud.sbJson.c_str());
        g_pendingHud.sbDataDirty = false;
    }
    if (g_pendingHud.resultDirty) {
        UI::Bridge::PushMatchResult(g_pendingHud.resultJson.c_str());
        g_pendingHud.resultDirty = false;
    }
    if (g_pendingHud.revertDirty) {
        UI::Bridge::PushDisplayRevertTick(g_pendingHud.revertSec);
        g_pendingHud.revertDirty = false;
    }
    for (const auto& k : g_pendingKills) {
        UI::Bridge::PushKillFeed(k[0], k[1], k[2], k[3]);
    }
    g_pendingKills.clear();

    g_renderer->Update();

    // Required in 1.4: drives CSS animations/transitions, smooth scrolling, rAF
    // and repaint scheduling. Symptom when missing: visual updates (hover,
    // scrolling) lag one input event behind (repaints piggyback on event dispatch).
    g_renderer->RefreshDisplay(0);

    g_renderer->Render();

    auto* surface = static_cast<ultralight::BitmapSurface*>(g_view->surface());
    if (!surface) return;

    // Upload only when something was actually repainted this frame
    // (canonical usage: clear dirty bounds after consuming)
    if (!surface->dirty_bounds().IsEmpty()) {
        void* pixels = surface->LockPixels();
        if (pixels) {
            g_d3d_renderer.UpdateFromBitmap(
                pixels,
                (int)surface->width(),
                (int)surface->height(),
                (int)surface->row_bytes()
            );
        }
        surface->UnlockPixels();
        surface->ClearDirtyBounds();
    }

    g_d3d_renderer.Draw();
}

void Finalize() {
#if defined(_DEBUG) || defined(DEBUG)
    HotReload::Stop();     // join the watcher thread before we tear down the view
#endif
    UI::Bridge::Unbind();  // drop the bridge's cached view before we release it
    g_d3d_renderer.Finalize();
    g_view     = nullptr;  // RefPtr release
    g_renderer = nullptr;
    g_filesystem.reset();
}

// Store only; flushed to JS inside Render (see PendingHud note above).
void PushHealth(int current, int maxHp) {
    g_pendingHud.health = current; g_pendingHud.healthMax = maxHp;
    g_pendingHud.healthDirty = true;
}
void PushAmmo(int current, int reserve) {
    g_pendingHud.ammo = current; g_pendingHud.ammoReserve = reserve;
    g_pendingHud.ammoDirty = true;
}
void PushScores(int red, int blue) {
    g_pendingHud.red = red; g_pendingHud.blue = blue;
    g_pendingHud.scoresDirty = true;
}
void PushMatchTimer(float secondsRemaining) {
    g_pendingHud.matchTime = secondsRemaining;
    g_pendingHud.timerDirty = true;
}
void PushKillFeed(int killerId, int victimId, int killerTeam, int victimTeam) {
    g_pendingKills.push_back({ killerId, victimId, killerTeam, victimTeam });
}
void PushScoreboard(const char* json) {
    if (json) { g_pendingHud.sbJson = json; g_pendingHud.sbDataDirty = true; }
}
void PushScoreboardVisible(bool visible) {
    g_pendingHud.sbVisible = visible; g_pendingHud.sbVisDirty = true;
}
void PushMatchResult(const char* json) {
    if (json) { g_pendingHud.resultJson = json; g_pendingHud.resultDirty = true; }
}
void PushDisplayRevertTick(int secondsLeft) {
    g_pendingHud.revertSec = secondsLeft; g_pendingHud.revertDirty = true;
}

void ProcessInput() {
    UIInputEvent ev;

    // Always drain the queue every frame — even in Display mode —
    // or events pile up (docs §5.2)
    while (UIInput::Dequeue(ev)) {
        if (!g_view) continue;

        // In RELATIVE mode (PLAY view control) mouse coordinates are meaningless;
        // drop them to prevent hover glitches
        if (ev.mouse_is_relative &&
            (ev.type == UIInputType::MouseMove ||
             ev.type == UIInputType::MouseDown ||
             ev.type == UIInputType::MouseUp   ||
             ev.type == UIInputType::Scroll)) {
            continue;
        }

        // Display: draw only, no input
        if (g_interactiveLevel == UI::InteractiveLevel::Display) continue;

        switch (ev.type) {
        case UIInputType::KeyDown: {
            // The Win32 constructor maps VK→GK internally (KeyEvent.h #ifdef _WIN32)
            // but does NOT fill key_identifier — without it DOM e.key is "Unidentified"
            ultralight::KeyEvent ke(ultralight::KeyEvent::kType_RawKeyDown,
                                    ev.wparam, ev.lparam, ev.is_system_key);
            ultralight::GetKeyIdentifierFromVirtualKeyCode(ke.virtual_key_code,
                                                           ke.key_identifier);
            g_view->FireKeyEvent(ke);
            break;
        }
        case UIInputType::KeyUp: {
            ultralight::KeyEvent ke(ultralight::KeyEvent::kType_KeyUp,
                                    ev.wparam, ev.lparam, ev.is_system_key);
            ultralight::GetKeyIdentifierFromVirtualKeyCode(ke.virtual_key_code,
                                                           ke.key_identifier);
            g_view->FireKeyEvent(ke);
            break;
        }
        case UIInputType::Char: {
            ultralight::KeyEvent ke(ultralight::KeyEvent::kType_Char,
                                    ev.wparam, ev.lparam, false);
            g_view->FireKeyEvent(ke);
            break;
        }
        case UIInputType::MouseMove:
        case UIInputType::MouseDown:
        case UIInputType::MouseUp: {
            ultralight::MouseEvent me;
            me.type = (ev.type == UIInputType::MouseMove) ? ultralight::MouseEvent::kType_MouseMoved
                    : (ev.type == UIInputType::MouseDown) ? ultralight::MouseEvent::kType_MouseDown
                                                          : ultralight::MouseEvent::kType_MouseUp;
            // Client (screen) pixels → page units (Ultralight expects page units).
            const float inv = (g_deviceScale > 0.0f) ? (1.0f / g_deviceScale) : 1.0f;
            me.x      = (int)(ev.mouse_x * inv);
            me.y      = (int)(ev.mouse_y * inv);
            me.button = (ultralight::MouseEvent::Button)ev.mouse_button;
            g_view->FireMouseEvent(me);
            break;
        }
        case UIInputType::Scroll: {
            ultralight::ScrollEvent se;
            se.type    = ultralight::ScrollEvent::kType_ScrollByPixel;
            se.delta_x = ev.scroll_delta_x;
            se.delta_y = ev.scroll_delta_y;
            g_view->FireScrollEvent(se);
            break;
        }
        }
    }
}

void SetInteractiveLevel(InteractiveLevel level) {
    g_interactiveLevel = level;
}

InteractiveLevel GetInteractiveLevel() {
    return g_interactiveLevel;
}

bool IsModalActive() {
    return g_interactiveLevel == InteractiveLevel::Modal;
}

void ShowPageDeferred(const char* name) {
    if (name) g_deferredPage = name;
}

void SetCurtain(bool visible) {
    if (!g_view) return;
    // Toggle the #curtain overlay; CSS animates the opacity fade (driven by
    // RefreshDisplay in Render). Called from the main loop (SceneTransition_Update),
    // never from inside a JS callback, so a direct EvaluateScript is safe — no
    // re-entrant script evaluation (cf. ShowPageDeferred).
    g_view->EvaluateScript(ultralight::String(visible ? "Curtain.show()" : "Curtain.hide()"));
}

void ShowPage(const char* name) {
    if (!g_view || !name) return;
    // Minimal escaping to guard against stray quotes. Replaced by the real Bridge in Slice D.
    std::string js = "Router.show('";
    for (const char* p = name; *p; ++p) {
        if (*p == '\'' || *p == '\\') js += '\\';
        js += *p;
    }
    js += "')";
    g_view->EvaluateScript(ultralight::String(js.c_str()));
}

// --- Hot reload helpers (Slice F) -------------------------------------------

void ReloadView() {
    if (!g_view) return;
    // OnDOMReady re-binds the bridge; router.js re-boots via game.getBootPage().
    g_view->Reload();
}

void ReloadStyles(const char* relPath) {
    if (!g_view || !relPath) return;
    std::string js = "window.reloadCSS && window.reloadCSS('";
    for (const char* p = relPath; *p; ++p) {
        if (*p == '\'' || *p == '\\') js += '\\';
        js += *p;
    }
    js += "')";
    g_view->EvaluateScript(ultralight::String(js.c_str()));
}

}  // namespace UI
