#include "ui_manager.h"
#include "ui_d3d11_renderer.h"
#include "ui_filesystem.h"

#include <Ultralight/Ultralight.h>
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/Renderer.h>
#include <Ultralight/View.h>
#include <Ultralight/Bitmap.h>
#include <Ultralight/platform/Surface.h>
#include <AppCore/Platform.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <memory>

namespace {

ultralight::RefPtr<ultralight::Renderer> g_renderer;
ultralight::RefPtr<ultralight::View>     g_view;
UI::D3D11BitmapRenderer                  g_d3d_renderer;
std::unique_ptr<UI::UIFileSystem>        g_filesystem;  // 需要长生命周期，Ultralight 持裸指针
int g_width = 0, g_height = 0;

std::string GetExeDirA() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(buf);
    return s.substr(0, s.find_last_of("\\/")) + "\\";
}

// Debug: exe 在 game_client/x64/Debug/，ui_src 在 game_client/ → 往上两层
// Release: ui_src 已镜像到 exe 同级的 resource/ui/
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

    // 1. Config —— resource_path_prefix 必须 exe-relative，否则 CWD 不对就崩
    //    （详见 docs/ultralight_integration.md §3.5）
    const std::string resPath = GetExeDirA() + "resources\\";
    ultralight::Config config;
    config.resource_path_prefix = resPath.c_str();
    ultralight::Platform::instance().set_config(config);

    // 2. FileSystem —— 自己的实现，按 Debug/Release 切根目录
    g_filesystem = std::make_unique<UI::UIFileSystem>(GetUiRoot());
    ultralight::Platform::instance().set_file_system(g_filesystem.get());

    // 3. FontLoader（必需，否则字体渲染不出来）
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
    vc.is_accelerated = false;        // CPU / BitmapSurface 路径
    vc.is_transparent = true;         // 让 HTML 未着色区域露出底下的 3D 场景
    vc.initial_device_scale = 1.0;
    g_view = g_renderer->CreateView((uint32_t)w, (uint32_t)h, vc, nullptr);
    if (!g_view) {
        OutputDebugStringA("[UI] CreateView failed\n");
        return;
    }

    g_view->LoadURL("file:///index.html");

    // 6. D3D11 渲染器
    if (!g_d3d_renderer.Initialize(dev, ctx, w, h)) {
        OutputDebugStringA("[UI] D3D11BitmapRenderer::Initialize failed\n");
    }
}

void Render() {
    if (!g_renderer || !g_view) return;

    g_renderer->Update();
    g_renderer->Render();

    auto* surface = static_cast<ultralight::BitmapSurface*>(g_view->surface());
    if (!surface) return;

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

    g_d3d_renderer.Draw();
}

void Finalize() {
    g_d3d_renderer.Finalize();
    g_view     = nullptr;  // RefPtr 释放
    g_renderer = nullptr;
    g_filesystem.reset();
}

void ShowPage(const char* name) {
    if (!g_view || !name) return;
    // 简单转义：避免单引号意外。Slice D 的正经 Bridge 会取代这个。
    std::string js = "Router.show('";
    for (const char* p = name; *p; ++p) {
        if (*p == '\'' || *p == '\\') js += '\\';
        js += *p;
    }
    js += "')";
    g_view->EvaluateScript(ultralight::String(js.c_str()));
}

}  // namespace UI
