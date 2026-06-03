#include "ui_d3d11_renderer.h"
#include <fstream>
#include <vector>
#include <string>
#include <windows.h>

namespace {

template <class T> void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

// 简单 .cso 读文件 helper
bool ReadFile(const char* path, std::vector<char>& out) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;
    auto sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    out.resize((size_t)sz);
    ifs.read(out.data(), sz);
    return ifs.good();
}

// D3D11 状态快照：Draw() 内部 save → 跑自己的 state → restore
struct D3D11StateSnapshot {
    ID3D11BlendState*         blend = nullptr;
    FLOAT                     blendFactor[4]{};
    UINT                      sampleMask = 0;
    ID3D11DepthStencilState*  depth = nullptr;
    UINT                      stencilRef = 0;
    ID3D11RasterizerState*    raster = nullptr;
    UINT                      vpCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT            viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    ID3D11VertexShader*       vs = nullptr;
    ID3D11PixelShader*        ps = nullptr;
    ID3D11InputLayout*        layout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY  topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11SamplerState*       samplers[1]{};
    ID3D11ShaderResourceView* srvs[1]{};
};

void SaveState(ID3D11DeviceContext* ctx, D3D11StateSnapshot& s) {
    ctx->OMGetBlendState(&s.blend, s.blendFactor, &s.sampleMask);
    ctx->OMGetDepthStencilState(&s.depth, &s.stencilRef);
    ctx->RSGetState(&s.raster);
    ctx->RSGetViewports(&s.vpCount, s.viewports);
    ctx->VSGetShader(&s.vs, nullptr, nullptr);
    ctx->PSGetShader(&s.ps, nullptr, nullptr);
    ctx->IAGetInputLayout(&s.layout);
    ctx->IAGetPrimitiveTopology(&s.topology);
    ctx->PSGetSamplers(0, 1, s.samplers);
    ctx->PSGetShaderResources(0, 1, s.srvs);
}

void RestoreState(ID3D11DeviceContext* ctx, D3D11StateSnapshot& s) {
    ctx->OMSetBlendState(s.blend, s.blendFactor, s.sampleMask);
    ctx->OMSetDepthStencilState(s.depth, s.stencilRef);
    ctx->RSSetState(s.raster);
    if (s.vpCount > 0) ctx->RSSetViewports(s.vpCount, s.viewports);
    ctx->VSSetShader(s.vs, nullptr, 0);
    ctx->PSSetShader(s.ps, nullptr, 0);
    ctx->IASetInputLayout(s.layout);
    ctx->IASetPrimitiveTopology(s.topology);
    ctx->PSSetSamplers(0, 1, s.samplers);
    ctx->PSSetShaderResources(0, 1, s.srvs);

    SafeRelease(s.blend);  SafeRelease(s.depth);  SafeRelease(s.raster);
    SafeRelease(s.vs);     SafeRelease(s.ps);     SafeRelease(s.layout);
    SafeRelease(s.samplers[0]); SafeRelease(s.srvs[0]);
}

}  // namespace

namespace UI {

bool D3D11BitmapRenderer::Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int w, int h) {
    m_device  = dev;
    m_context = ctx;
    m_width   = w;
    m_height  = h;

    // 1. Dynamic BGRA texture
    D3D11_TEXTURE2D_DESC td{};
    td.Width      = w;
    td.Height     = h;
    td.MipLevels  = 1;
    td.ArraySize  = 1;
    td.Format     = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage      = D3D11_USAGE_DYNAMIC;
    td.BindFlags  = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &m_texture))) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = td.Format;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MipLevels = 1;
    if (FAILED(dev->CreateShaderResourceView(m_texture, &sd, &m_srv))) return false;

    // 2. Shaders
    std::vector<char> vsBlob, psBlob;
    if (!ReadFile("resource/shader/ui/ui_vs.cso", vsBlob)) {
        OutputDebugStringA("[UI] Failed to load ui_vs.cso\n");
        return false;
    }
    if (!ReadFile("resource/shader/ui/ui_ps.cso", psBlob)) {
        OutputDebugStringA("[UI] Failed to load ui_ps.cso\n");
        return false;
    }
    if (FAILED(dev->CreateVertexShader(vsBlob.data(), vsBlob.size(), nullptr, &m_vs))) return false;
    if (FAILED(dev->CreatePixelShader (psBlob.data(), psBlob.size(), nullptr, &m_ps))) return false;

    // 3. Sampler (linear clamp)
    D3D11_SAMPLER_DESC sa{};
    sa.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sa.AddressU = sa.AddressV = sa.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sa.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sa.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(dev->CreateSamplerState(&sa, &m_sampler))) return false;

    // 4. Blend state: premultiplied alpha
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable    = TRUE;
    bd.RenderTarget[0].SrcBlend       = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&bd, &m_blend))) return false;

    // 5. Depth: disabled (UI always on top)
    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable    = FALSE;
    dd.StencilEnable  = FALSE;
    if (FAILED(dev->CreateDepthStencilState(&dd, &m_depth))) return false;

    // 6. Rasterizer: no cull (full-screen triangle is fine either way)
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(dev->CreateRasterizerState(&rd, &m_raster))) return false;

    return true;
}

void D3D11BitmapRenderer::Finalize() {
    SafeRelease(m_raster);
    SafeRelease(m_depth);
    SafeRelease(m_blend);
    SafeRelease(m_sampler);
    SafeRelease(m_ps);
    SafeRelease(m_vs);
    SafeRelease(m_srv);
    SafeRelease(m_texture);
    m_device = nullptr;
    m_context = nullptr;
}

void D3D11BitmapRenderer::UpdateFromBitmap(const void* src, int srcW, int srcH, int srcRowBytes) {
    if (!m_texture || !m_context) return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_context->Map(m_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;

    const int copyH = (srcH < m_height) ? srcH : m_height;
    const int copyRowBytes = (srcRowBytes < (int)mapped.RowPitch) ? srcRowBytes : (int)mapped.RowPitch;
    const auto* s = (const uint8_t*)src;
    auto* d       = (uint8_t*)mapped.pData;
    for (int y = 0; y < copyH; ++y) {
        memcpy(d + y * mapped.RowPitch, s + y * srcRowBytes, copyRowBytes);
    }
    m_context->Unmap(m_texture, 0);
}

void D3D11BitmapRenderer::Draw() {
    if (!m_context || !m_vs || !m_ps) return;

    D3D11StateSnapshot snap;
    SaveState(m_context, snap);

    // 全屏 viewport
    D3D11_VIEWPORT vp{};
    vp.Width    = (FLOAT)m_width;
    vp.Height   = (FLOAT)m_height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    // 自己的 state
    const FLOAT bf[4] = { 0,0,0,0 };
    m_context->OMSetBlendState(m_blend, bf, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(m_depth, 0);
    m_context->RSSetState(m_raster);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vs, nullptr, 0);
    m_context->PSSetShader(m_ps, nullptr, 0);
    m_context->PSSetSamplers(0, 1, &m_sampler);
    m_context->PSSetShaderResources(0, 1, &m_srv);

    // 全屏三角形：3 顶点，VS 用 SV_VertexID 生成
    m_context->Draw(3, 0);

    RestoreState(m_context, snap);
}

}  // namespace UI
