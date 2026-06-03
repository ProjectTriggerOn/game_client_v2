#include "ui_manager.h"
#include "ui_d3d11_renderer.h"

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

namespace {

ultralight::RefPtr<ultralight::Renderer> g_renderer;
ultralight::RefPtr<ultralight::View>     g_view;
UI::D3D11BitmapRenderer                  g_d3d_renderer;
int g_width = 0, g_height = 0;

std::string GetExeDirA() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(buf);
    return s.substr(0, s.find_last_of("\\/")) + "\\";
}

// Slice A 用的硬编码 HTML。后续切片改为 UIFileSystem 从 ui_src/index.html 加载。
const char* kSliceAHtml = R"HTML(
<!DOCTYPE html>
<html>
<head><style>
  html, body { margin:0; padding:0; height:100%; }
  body {
    background: red;
    color: white;
    font: bold 64px sans-serif;
    display: flex; align-items: center; justify-content: center;
  }
</style></head>
<body>Hello Ultralight</body>
</html>
)HTML";

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

    // 2. FileSystem 占位（Slice B 替换为我们自己的 UIFileSystem）
    ultralight::Platform::instance().set_file_system(
        ultralight::GetPlatformFileSystem(GetExeDirA().c_str())
    );

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

    g_view->LoadHTML(kSliceAHtml);

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
}

}  // namespace UI
