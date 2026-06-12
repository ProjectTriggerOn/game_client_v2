#include "ui_manager.h"
#include "ui_d3d11_renderer.h"
#include "ui_filesystem.h"
#include "ui_input_queue.h"

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

namespace {

ultralight::RefPtr<ultralight::Renderer> g_renderer;
ultralight::RefPtr<ultralight::View>     g_view;
UI::D3D11BitmapRenderer                  g_d3d_renderer;
std::unique_ptr<UI::UIFileSystem>        g_filesystem;  // must outlive the View; Ultralight keeps a raw pointer
int g_width = 0, g_height = 0;

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

void Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int w, int h) {
    g_width  = w;
    g_height = h;

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
    vc.initial_device_scale = 1.0;
    g_view = g_renderer->CreateView((uint32_t)w, (uint32_t)h, vc, nullptr);
    if (!g_view) {
        OutputDebugStringA("[UI] CreateView failed\n");
        return;
    }

    g_view->set_view_listener(&g_viewListener);   // console.log → VS Output window

    g_view->LoadURL("file:///index.html");

    // 6. D3D11 renderer
    if (!g_d3d_renderer.Initialize(dev, ctx, w, h)) {
        OutputDebugStringA("[UI] D3D11BitmapRenderer::Initialize failed\n");
    }
}

void Render() {
    if (!g_renderer || !g_view) return;

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
    g_d3d_renderer.Finalize();
    g_view     = nullptr;  // RefPtr release
    g_renderer = nullptr;
    g_filesystem.reset();
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
            me.x      = ev.mouse_x;
            me.y      = ev.mouse_y;
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

}  // namespace UI
