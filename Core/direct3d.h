#ifndef DIRECT3D_H
#define DIRECT3D_H

#include <d3d11.h>
#include <Windows.h>



// �Z�[�t�����[�X�}�N��
#define SAFE_RELEASE(o) if (o) { (o)->Release(); o = NULL; }


bool Direct3D_Initialize(HWND hWnd); // Direct3D�̏�����
void Direct3D_Finalize(); // Direct3D�̏I������

void Direct3D_Clear(); // �o�b�N�o�b�t�@�̃N���A
void Direct3D_Present(); // �o�b�N�o�b�t�@�̕\��

unsigned int Direct3D_GetBackBufferWidth();// バックバッファの幅を取得
unsigned int Direct3D_GetBackBufferHeight(); // バックバッファの幅と高さを取得

// --- Display settings support (Core/display_manager.cpp) ---------------------
// Resize the swap-chain back buffer (FLIP-model correct: releases every back-
// buffer reference, ResizeBuffers, then rebuilds RTV/depth/viewport). Returns
// false on a zero size or a failed ResizeBuffers. Aspect ratio self-corrects
// because every camera recomputes it from the back-buffer size each frame.
bool Direct3D_Resize(unsigned int width, unsigned int height);

// Present sync interval: 1 = VSync on, 0 = off (tearing when supported + windowed).
void Direct3D_SetVSyncInterval(unsigned int interval);

// Exclusive-fullscreen control wrappers (DisplayManager never touches the
// IDXGISwapChain directly). `output` may be null (uses the window's output).
bool Direct3D_SetFullscreenState(bool fullscreen, IDXGIOutput* output);
bool Direct3D_ResizeTarget(const DXGI_MODE_DESC* mode);
bool Direct3D_IsFullscreen();
// Snap a requested mode to the closest one the output supports (before entering
// exclusive fullscreen). Returns false if no device/swap-chain yet.
bool Direct3D_FindClosestMode(IDXGIOutput* output, const DXGI_MODE_DESC* want, DXGI_MODE_DESC* got);

ID3D11Device* Direct3D_GetDevice();
ID3D11DeviceContext* Direct3D_GetDeviceContext();

void Direct3D_SetDepthEnable(bool enable);
void Direct3D_SetCullMode(D3D11_CULL_MODE mode);

#endif // DIRECT3D_H
