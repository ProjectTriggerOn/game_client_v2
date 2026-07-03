#pragma once
//=============================================================================
// display_manager.h
//
// Owns everything about the window's display state: mode (windowed / borderless
// / exclusive), resolution, target monitor, and the DXGI enumeration behind the
// settings-page dropdowns. Orchestrates Direct3D_* (swap-chain resize / mode
// switch), UI::Resize and the Config layer.
//
// Threading / re-entrancy: the JS bridge only STAGES requests (RequestApply /
// RequestConfirm / RequestRevert); the actual window + swap-chain mutation runs
// in Update() on the main loop. Poking the window from inside a JS callback
// would pump messages and re-enter WndProc mid-script (same hazard the HUD
// PendingHud buffer avoids).
//=============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace Display {

enum class Mode { Windowed, Borderless, Exclusive };

// Capture the DXGI factory/adapter/outputs and apply the persisted display mode
// (no revert countdown — startup is trusted). Call after Direct3D + UI are up.
void Initialize(HWND hWnd);

// Leave exclusive fullscreen (if any) and release the cached DXGI objects.
// Must run before Direct3D_Finalize.
void Finalize();

// Main-loop tick: consume any staged request and drive the 15s revert countdown.
// Call once per frame BEFORE Direct3D_Clear so a resize precedes this frame's draw.
void Update(double dt);

// Read-only cached snapshot (monitors[] + modes[] + current settings) as JSON.
// Safe to call synchronously from a JS callback — never touches the swap chain.
std::string GetInfoJson();

// --- Bridge-facing (called from JS callbacks; STORE-only) --------------------
void RequestApply(Mode m, int width, int height, int monitorIdx);
void RequestConfirm();
void RequestRevert();

// --- WndProc-facing ----------------------------------------------------------
// Single idempotent resize funnel: resizes the swap chain + UI + mouse clip to
// a new client size (guards zero/duplicate sizes).
void OnWindowResize(int w, int h);
// Exclusive-fullscreen alt-tab handling (deferred to the next Update).
void OnActivateApp(bool active);

bool IsInitialized();

}  // namespace Display
