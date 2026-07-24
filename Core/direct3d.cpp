#include <d3d11.h>
#include <dxgi1_5.h>   // IDXGIFactory5 / DXGI_FEATURE_PRESENT_ALLOW_TEARING
#include "direct3d.h"
#include "debug_ostream.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Direct3D interfaces (singleton state)
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;

// Swap-chain flags decided at creation. ResizeBuffers MUST reuse the exact same
// flags (mandatory for ALLOW_MODE_SWITCH / ALLOW_TEARING). VSync interval and
// tearing capability drive Present.
static UINT g_SwapChainFlags = 0;
static UINT g_VSyncInterval  = 1;      // 1 = VSync on (Config subscriber overrides)
static bool g_AllowTearing   = false;  // true only when the OS/driver supports it
static ID3D11BlendState* g_pBlendStateMultiply = nullptr;
static ID3D11DepthStencilState* g_pDepthStencilStateDepthDisable = nullptr;
static ID3D11DepthStencilState* g_pDepthStencilStateDepthEnable = nullptr;
static ID3D11RasterizerState* g_pRasterizerStateCullBack = nullptr;
static ID3D11RasterizerState* g_pRasterizerStateCullNone = nullptr;

// Back buffer state
static ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
static ID3D11Texture2D* g_pDepthStencilBuffer = nullptr;
static ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
static D3D11_TEXTURE2D_DESC g_BackBufferDesc{};
static D3D11_VIEWPORT g_Viewport{};

static bool configureBackBuffer();
static void releaseBackBuffer();


// Probe OS/driver support for tearing (uncapped VSync-off) via a temporary
// factory, before the device exists. Safe no-op on pre-Win10 (QI just fails).
static bool QueryTearingSupport()
{
    IDXGIFactory5* factory5 = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory5))) || !factory5)
        return false;
    BOOL allow = FALSE;
    HRESULT hr = factory5->CheckFeatureSupport(
        DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow));
    factory5->Release();
    return SUCCEEDED(hr) && allow;
}

bool Direct3D_Initialize(HWND hWnd)
{
    // Mode switch is required for exclusive-fullscreen resolution changes;
    // tearing is added only when supported (used by an uncapped, VSync-off cap).
    g_AllowTearing   = QueryTearingSupport();
    g_SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
                     | (g_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u);

    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.BufferCount = 2;
    // BufferDesc.Width/Height left at 0: swap chain auto-sizes to the window
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swap_chain_desc.OutputWindow = hWnd;
    swap_chain_desc.Flags = g_SwapChainFlags;

	UINT device_flags = 0;

#if defined(DEBUG) || defined(_DEBUG)
    device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
 
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        device_flags,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &g_pSwapChain,
        &g_pDevice,
        &feature_level,
        &g_pDeviceContext);

    if (FAILED(hr)) {
		MessageBox(hWnd, "Failed to initialize Direct3D.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

	// Own Alt+Enter ourselves: DisplayManager drives all mode switches, so DXGI
	// must not toggle fullscreen behind its back (that would desync g_current).
	{
		IDXGIFactory* factory = nullptr;
		if (SUCCEEDED(g_pSwapChain->GetParent(IID_PPV_ARGS(&factory))) && factory) {
			factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
			factory->Release();
		}
	}

	if (!configureBackBuffer()) {
		MessageBox(hWnd, "Failed to configure the back buffer.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	// Blend state: standard SrcAlpha / InvSrcAlpha blending
	D3D11_BLEND_DESC bd = {};
	bd.AlphaToCoverageEnable = FALSE;
	bd.IndependentBlendEnable = FALSE;
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	g_pDevice->CreateBlendState(&bd, &g_pBlendStateMultiply);

	float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	g_pDeviceContext->OMSetBlendState(g_pBlendStateMultiply, blend_factor, 0xffffffff);

	// Depth-stencil states (one with depth test disabled, one enabled)
	D3D11_DEPTH_STENCIL_DESC dsd = {};
	dsd.DepthFunc = D3D11_COMPARISON_LESS;
	dsd.StencilEnable = FALSE;
	dsd.DepthEnable = FALSE;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	g_pDevice->CreateDepthStencilState(&dsd, &g_pDepthStencilStateDepthDisable);

	dsd.DepthEnable = TRUE;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	g_pDevice->CreateDepthStencilState(&dsd, &g_pDepthStencilStateDepthEnable);

	Direct3D_SetDepthEnable(true);

	// Rasterizer states (back-face culling + no culling)
	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_BACK;
	rd.DepthClipEnable = TRUE;
	rd.MultisampleEnable = FALSE;
	g_pDevice->CreateRasterizerState(&rd, &g_pRasterizerStateCullBack);
	g_pDeviceContext->RSSetState(g_pRasterizerStateCullBack);

	rd.CullMode = D3D11_CULL_NONE;
	hr = g_pDevice->CreateRasterizerState(&rd, &g_pRasterizerStateCullNone);
	if (FAILED(hr)) return false;

    return true;
}

void Direct3D_Finalize()
{
	SAFE_RELEASE(g_pDepthStencilStateDepthDisable)
	SAFE_RELEASE(g_pDepthStencilStateDepthEnable)
	SAFE_RELEASE(g_pBlendStateMultiply)
	SAFE_RELEASE(g_pRasterizerStateCullBack)
	SAFE_RELEASE(g_pRasterizerStateCullNone)

	releaseBackBuffer();

	// Releasing a swap chain while in exclusive fullscreen is illegal (DXGI
	// asserts). Always drop back to windowed first.
	if (g_pSwapChain) g_pSwapChain->SetFullscreenState(FALSE, nullptr);

	SAFE_RELEASE(g_pSwapChain)
	SAFE_RELEASE(g_pDeviceContext)
	SAFE_RELEASE(g_pDevice)
}

bool Direct3D_Resize(unsigned int width, unsigned int height)
{
	if (!g_pSwapChain || width == 0 || height == 0) return false;

	// FLIP model requires every outstanding back-buffer reference released before
	// ResizeBuffers — releaseBackBuffer() drops the RTV + depth stencil (the only
	// GetBuffer references we hold).
	releaseBackBuffer();

	HRESULT hr = g_pSwapChain->ResizeBuffers(
		0, width, height, DXGI_FORMAT_UNKNOWN, g_SwapChainFlags);
	if (FAILED(hr)) {
		hal::dout << "ResizeBuffers failed" << std::endl;
		return false;
	}

	// Rebuilds RTV/depth/viewport and re-reads the new size into g_BackBufferDesc,
	// so Direct3D_GetBackBufferWidth/Height (and every camera's aspect) self-correct.
	return configureBackBuffer();
}

void Direct3D_SetVSyncInterval(unsigned int interval)
{
	g_VSyncInterval = interval;
}

bool Direct3D_SetFullscreenState(bool fullscreen, IDXGIOutput* output)
{
	if (!g_pSwapChain) return false;
	return SUCCEEDED(g_pSwapChain->SetFullscreenState(fullscreen ? TRUE : FALSE, output));
}

bool Direct3D_ResizeTarget(const DXGI_MODE_DESC* mode)
{
	if (!g_pSwapChain || !mode) return false;
	return SUCCEEDED(g_pSwapChain->ResizeTarget(mode));
}

bool Direct3D_IsFullscreen()
{
	if (!g_pSwapChain) return false;
	BOOL fs = FALSE;
	g_pSwapChain->GetFullscreenState(&fs, nullptr);
	return fs == TRUE;
}

bool Direct3D_FindClosestMode(IDXGIOutput* output, const DXGI_MODE_DESC* want, DXGI_MODE_DESC* got)
{
	if (!output || !want || !got || !g_pDevice) return false;
	return SUCCEEDED(output->FindClosestMatchingMode(want, got, g_pDevice));
}

void Direct3D_Clear()
{
	float clear_color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	g_pDeviceContext->ClearRenderTargetView(g_pRenderTargetView, clear_color);
	g_pDeviceContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
}

void Direct3D_Present()
{
	// Tearing (true uncapped) is only legal with sync interval 0, when the OS
	// supports it, and NOT in exclusive fullscreen.
	UINT flags = (g_VSyncInterval == 0 && g_AllowTearing && !Direct3D_IsFullscreen())
		? DXGI_PRESENT_ALLOW_TEARING : 0u;
	g_pSwapChain->Present(g_VSyncInterval, flags);
}

unsigned int Direct3D_GetBackBufferWidth()
{
	return g_BackBufferDesc.Width;
}

unsigned int Direct3D_GetBackBufferHeight()
{
	return g_BackBufferDesc.Height;
}

ID3D11Device* Direct3D_GetDevice()
{
	return g_pDevice;
}

ID3D11DeviceContext* Direct3D_GetDeviceContext()
{
	return g_pDeviceContext;
}

void Direct3D_SetDepthEnable(bool enable)
{
	if (enable)
	{
		g_pDeviceContext->OMSetDepthStencilState(g_pDepthStencilStateDepthEnable, NULL);
	}
	else
	{
		g_pDeviceContext->OMSetDepthStencilState(g_pDepthStencilStateDepthDisable, NULL);
	}
}

void Direct3D_SetCullMode(D3D11_CULL_MODE mode)
{
	switch (mode)
	{
	case D3D11_CULL_BACK:
		g_pDeviceContext->RSSetState(g_pRasterizerStateCullBack);
		break;
	case D3D11_CULL_NONE:
		g_pDeviceContext->RSSetState(g_pRasterizerStateCullNone);
		break;
	default:
		break;
	}
}

bool configureBackBuffer()
{
	HRESULT hr;

	ID3D11Texture2D* back_buffer_pointer = nullptr;

	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer_pointer);
	if (FAILED(hr)) {
		hal::dout << "Failed to get back buffer" << std::endl;
		return false;
	}

	hr = g_pDevice->CreateRenderTargetView(back_buffer_pointer, nullptr, &g_pRenderTargetView);
	if (FAILED(hr)) {
		back_buffer_pointer->Release();
		hal::dout << "Failed to create render target view" << std::endl;
		return false;
	}

	back_buffer_pointer->GetDesc(&g_BackBufferDesc);
	back_buffer_pointer->Release();

	D3D11_TEXTURE2D_DESC depth_stencil_desc{};
	depth_stencil_desc.Width = g_BackBufferDesc.Width;
	depth_stencil_desc.Height = g_BackBufferDesc.Height;
	depth_stencil_desc.MipLevels = 1;
	depth_stencil_desc.ArraySize = 1;
	depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_desc.SampleDesc.Count = 1;
	depth_stencil_desc.SampleDesc.Quality = 0;
	depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
	depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depth_stencil_desc.CPUAccessFlags = 0;
	depth_stencil_desc.MiscFlags = 0;
	hr = g_pDevice->CreateTexture2D(&depth_stencil_desc, nullptr, &g_pDepthStencilBuffer);
	if (FAILED(hr)) {
		hal::dout << "Failed to create depth-stencil buffer" << std::endl;
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = depth_stencil_desc.Format;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	depth_stencil_view_desc.Flags = 0;
	hr = g_pDevice->CreateDepthStencilView(g_pDepthStencilBuffer, &depth_stencil_view_desc, &g_pDepthStencilView);
	if (FAILED(hr)) {
		hal::dout << "Failed to create depth-stencil view" << std::endl;
		return false;
	}

	g_Viewport.TopLeftX = 0.0f;
	g_Viewport.TopLeftY = 0.0f;
	g_Viewport.Width = static_cast<FLOAT>(g_BackBufferDesc.Width);
	g_Viewport.Height = static_cast<FLOAT>(g_BackBufferDesc.Height);
	g_Viewport.MinDepth = 0.0f;
	g_Viewport.MaxDepth = 1.0f;
	g_pDeviceContext->RSSetViewports(1, &g_Viewport);

	return true;
}

void releaseBackBuffer()
{
	SAFE_RELEASE(g_pRenderTargetView)
	SAFE_RELEASE(g_pDepthStencilBuffer)
	SAFE_RELEASE(g_pDepthStencilView)
}
