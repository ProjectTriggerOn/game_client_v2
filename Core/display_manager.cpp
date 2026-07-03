#include "display_manager.h"
#include "direct3d.h"
#include "ui_manager.h"
#include "config.h"
#include "mouse.h"

#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <utility>
#pragma comment(lib, "dxgi.lib")

namespace {

// --- Resolved display state --------------------------------------------------
// w/h are always concrete (the resolved render size). reqW/reqH are what the
// user asked for (0 = "Native/Auto"); they are persisted verbatim so the Auto
// semantic survives a restart.
struct DisplayState {
    Display::Mode mode = Display::Mode::Windowed;
    int w = 1280, h = 720;    // resolved
    int monitor = 0;
    int reqW = 0, reqH = 0;   // requested (0 = native)
};

HWND g_hWnd = nullptr;
bool g_initialized = false;

DisplayState g_current;    // live now
DisplayState g_lastGood;   // to revert to

// Staged requests (store-then-act; applied in Update on the main loop).
bool g_pendingApply = false;
DisplayState g_pendingState;
bool g_pendingConfirm = false;
bool g_pendingRevert  = false;

// Persisted mode applied once on the first Update (after ShowWindow), no countdown.
bool g_startupPending = false;
DisplayState g_startupState;

// 15s revert countdown.
bool   g_countingDown = false;
double g_revertRemaining = 0.0;
int    g_lastPushedSec = -1;
const double REVERT_SECONDS = 15.0;

// Exclusive alt-tab: deferred to Update to avoid re-entering WndProc mid-message.
bool g_restoreExclusive = false;
bool g_pendingActivate = false;
bool g_activateState = false;

// --- Cached DXGI enumeration -------------------------------------------------
struct MonInfo {
    std::string deviceName;                    // e.g. \\.\DISPLAY1
    RECT rcMonitor{};                          // full bounds (physical px)
    RECT rcWork{};                             // work area (for windowed centering)
    bool primary = false;
    int  nativeW = 0, nativeH = 0, nativeHz = 0;
    std::vector<std::pair<int, int>> modes;    // deduped (w,h), descending
};

IDXGIFactory* g_factory = nullptr;
IDXGIAdapter* g_adapter = nullptr;
std::vector<IDXGIOutput*> g_outputs;
std::vector<MonInfo> g_monitors;

// --- helpers -----------------------------------------------------------------

const char* ModeToString(Display::Mode m) {
    switch (m) {
    case Display::Mode::Borderless: return "borderless";
    case Display::Mode::Exclusive:  return "exclusive";
    default:                        return "windowed";
    }
}

Display::Mode ParseMode(const std::string& s) {
    if (s == "borderless") return Display::Mode::Borderless;
    if (s == "exclusive")  return Display::Mode::Exclusive;
    return Display::Mode::Windowed;
}

std::string JsonEscape(const std::string& in) {
    std::string o;
    o.reserve(in.size() + 4);
    for (char c : in) {
        if (c == '\\' || c == '"') o += '\\';
        o += c;
    }
    return o;
}

void EnumerateMonitors() {
    ID3D11Device* dev = Direct3D_GetDevice();
    if (!dev) return;

    IDXGIDevice* dxgiDev = nullptr;
    if (FAILED(dev->QueryInterface(IID_PPV_ARGS(&dxgiDev))) || !dxgiDev) return;
    if (FAILED(dxgiDev->GetAdapter(&g_adapter)) || !g_adapter) { dxgiDev->Release(); return; }
    dxgiDev->Release();
    g_adapter->GetParent(IID_PPV_ARGS(&g_factory));

    for (UINT i = 0; ; ++i) {
        IDXGIOutput* out = nullptr;
        if (g_adapter->EnumOutputs(i, &out) == DXGI_ERROR_NOT_FOUND) break;
        if (!out) break;
        g_outputs.push_back(out);

        DXGI_OUTPUT_DESC od{};
        out->GetDesc(&od);

        MonInfo mi{};
        char name[128] = {};
        WideCharToMultiByte(CP_UTF8, 0, od.DeviceName, -1, name, sizeof(name), nullptr, nullptr);
        mi.deviceName = name;

        mi.rcMonitor = od.DesktopCoordinates;
        mi.rcWork    = od.DesktopCoordinates;
        MONITORINFO monInfo{};
        monInfo.cbSize = sizeof(monInfo);
        if (GetMonitorInfo(od.Monitor, &monInfo)) {
            mi.rcMonitor = monInfo.rcMonitor;
            mi.rcWork    = monInfo.rcWork;
            mi.primary   = (monInfo.dwFlags & MONITORINFOF_PRIMARY) != 0;
        }
        mi.nativeW = mi.rcMonitor.right - mi.rcMonitor.left;
        mi.nativeH = mi.rcMonitor.bottom - mi.rcMonitor.top;

        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsW(od.DeviceName, ENUM_CURRENT_SETTINGS, &dm))
            mi.nativeHz = (int)dm.dmDisplayFrequency;

        UINT num = 0;
        const UINT enumFlags = DXGI_ENUM_MODES_INTERLACED | DXGI_ENUM_MODES_SCALING;
        out->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, enumFlags, &num, nullptr);
        if (num > 0) {
            std::vector<DXGI_MODE_DESC> list(num);
            out->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, enumFlags, &num, list.data());
            for (const auto& md : list) {
                const int w = (int)md.Width, h = (int)md.Height;
                bool found = false;
                for (const auto& p : mi.modes)
                    if (p.first == w && p.second == h) { found = true; break; }
                if (!found) mi.modes.emplace_back(w, h);
            }
            std::sort(mi.modes.begin(), mi.modes.end(),
                [](const std::pair<int,int>& a, const std::pair<int,int>& b) {
                    if (a.first != b.first) return a.first > b.first;
                    return a.second > b.second;
                });
        }
        g_monitors.push_back(std::move(mi));
    }
}

// Fill resolved w/h from reqW/reqH. "Native/Auto" (reqW<=0) resolves per mode:
//   borderless / exclusive → the monitor's native desktop resolution
//   windowed               → the legacy [client] window size (a desktop-sized
//                            *window* would overflow its title bar)
void ResolveSize(DisplayState& s) {
    if (s.monitor < 0 || s.monitor >= (int)g_monitors.size()) s.monitor = 0;
    int nativeW = 1280, nativeH = 720;
    if (!g_monitors.empty()) {
        nativeW = g_monitors[s.monitor].nativeW;
        nativeH = g_monitors[s.monitor].nativeH;
    }

    if (s.mode == Display::Mode::Borderless) {
        s.w = nativeW; s.h = nativeH;   // borderless is always native
    } else if (s.reqW > 0 && s.reqH > 0) {
        s.w = s.reqW;  s.h = s.reqH;    // explicit resolution
    } else if (s.mode == Display::Mode::Exclusive) {
        s.w = nativeW; s.h = nativeH;   // exclusive auto = native
    } else {
        auto& cfg = Config::GetInstance();  // windowed auto = legacy window size
        s.w = cfg.GetInt("client", "window_width",  1920);
        s.h = cfg.GetInt("client", "window_height", 1080);
    }
}

RECT MonitorRect(int idx) {
    if (idx >= 0 && idx < (int)g_monitors.size()) return g_monitors[idx].rcMonitor;
    return RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}
RECT WorkRect(int idx) {
    if (idx >= 0 && idx < (int)g_monitors.size()) return g_monitors[idx].rcWork;
    return RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

void ApplyWindowed(const DisplayState& s) {
    if (Direct3D_IsFullscreen()) Direct3D_SetFullscreenState(false, nullptr);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    SetWindowLongPtrW(g_hWnd, GWL_STYLE, (LONG_PTR)style);

    RECT r{ 0, 0, s.w, s.h };
    AdjustWindowRect(&r, style, FALSE);
    const int ww = r.right - r.left, wh = r.bottom - r.top;

    const RECT work = WorkRect(s.monitor);
    const int mw = work.right - work.left, mh = work.bottom - work.top;
    const int x = work.left + std::max((mw - ww) / 2, 0);
    const int y = work.top  + std::max((mh - wh) / 2, 0);

    SetWindowPos(g_hWnd, HWND_NOTOPMOST, x, y, ww, wh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    Display::OnWindowResize(s.w, s.h);
}

void ApplyBorderless(const DisplayState& s) {
    if (Direct3D_IsFullscreen()) Direct3D_SetFullscreenState(false, nullptr);

    const RECT mon = MonitorRect(s.monitor);
    SetWindowLongPtrW(g_hWnd, GWL_STYLE, (LONG_PTR)(WS_POPUP | WS_VISIBLE));
    SetWindowPos(g_hWnd, HWND_TOP, mon.left, mon.top,
                 mon.right - mon.left, mon.bottom - mon.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    Display::OnWindowResize(mon.right - mon.left, mon.bottom - mon.top);
}

// Returns true only if exclusive fullscreen actually engaged. SetFullscreenState
// can legitimately fail (another app holds exclusive, RDP session, a mode the
// output can't take); returning success then would desync g_current from the
// real DXGI state and re-fire on every alt-tab.
bool ApplyExclusive(const DisplayState& s) {
    IDXGIOutput* out = (s.monitor >= 0 && s.monitor < (int)g_outputs.size())
                     ? g_outputs[s.monitor] : nullptr;

    DXGI_MODE_DESC want{};
    want.Width  = (UINT)s.w;
    want.Height = (UINT)s.h;
    want.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    want.RefreshRate.Numerator = 0;
    want.RefreshRate.Denominator = 0;   // let DXGI pick the numerator/denominator

    DXGI_MODE_DESC got = want;
    Direct3D_FindClosestMode(out, &want, &got);

    Direct3D_ResizeTarget(&got);                             // size the target to the mode
    const bool entered = Direct3D_SetFullscreenState(true, out);  // enter true exclusive

    if (entered) {
        // MS-recommended: a second ResizeTarget with refresh cleared avoids the
        // "fullscreen state lost" flicker some drivers exhibit.
        DXGI_MODE_DESC got2 = got;
        got2.RefreshRate.Numerator = 0;
        got2.RefreshRate.Denominator = 0;
        Direct3D_ResizeTarget(&got2);
    }

    Display::OnWindowResize((int)got.Width, (int)got.Height);
    // Reconcile against the REAL DXGI state, not just the call's return.
    return entered && Direct3D_IsFullscreen();
}

// Returns the state that was ACTUALLY applied. If an exclusive transition fails,
// falls back to windowed and returns that — so g_current never claims a mode DXGI
// is not in (which would mislead GetInfoJson/PersistCurrent and re-fire on alt-tab).
DisplayState ApplyDisplayState(const DisplayState& s) {
    DisplayState eff = s;
    switch (s.mode) {
    case Display::Mode::Windowed:   ApplyWindowed(s);   break;
    case Display::Mode::Borderless: ApplyBorderless(s); break;
    case Display::Mode::Exclusive:
        if (!ApplyExclusive(s)) {
            eff.mode = Display::Mode::Windowed;   // exclusive unavailable → safe fallback
            ResolveSize(eff);
            ApplyWindowed(eff);
        }
        break;
    }
    g_restoreExclusive = (eff.mode == Display::Mode::Exclusive);
    return eff;
}

void PersistCurrent() {
    auto& cfg = Config::GetInstance();
    cfg.Set("display.mode", ConfigValue(std::string(ModeToString(g_current.mode))));
    // Borderless is always native → persist 0 (Auto) so it tracks the desktop.
    const int pw = (g_current.mode == Display::Mode::Borderless) ? 0 : g_current.reqW;
    const int ph = (g_current.mode == Display::Mode::Borderless) ? 0 : g_current.reqH;
    cfg.Set("display.width",  ConfigValue((int64_t)pw));
    cfg.Set("display.height", ConfigValue((int64_t)ph));
    cfg.Set("display.monitor_index", ConfigValue((int64_t)g_current.monitor));
    // Config has a single user layer, so this commits the whole layer to disk —
    // same as the settings-page SAVE button. Any live-applied but un-SAVEd keys
    // (sensitivity/vsync/fov/…) are flushed too; that mirrors SAVE and is
    // intentional given there is no separate per-key staging buffer.
    cfg.SaveToFile();
}

}  // namespace

namespace Display {

void Initialize(HWND hWnd) {
    g_hWnd = hWnd;
    EnumerateMonitors();

    // The window was already created windowed at the config size (game_window.cpp),
    // so the live state right now is exactly that.
    g_current.mode    = Mode::Windowed;
    g_current.w       = (int)Direct3D_GetBackBufferWidth();
    g_current.h       = (int)Direct3D_GetBackBufferHeight();
    g_current.reqW    = g_current.w;
    g_current.reqH    = g_current.h;
    g_current.monitor = 0;
    g_lastGood = g_current;
    g_initialized = true;

    // Resolve the persisted target; if it differs, apply it on the first Update
    // (after ShowWindow) with no countdown.
    auto& cfg = Config::GetInstance();
    DisplayState target;
    target.mode    = ParseMode(cfg.Get("display.mode").AsString());
    target.reqW    = (int)cfg.Get("display.width").AsInt();
    target.reqH    = (int)cfg.Get("display.height").AsInt();
    target.monitor = (int)cfg.Get("display.monitor_index").AsInt();
    ResolveSize(target);

    if (target.mode != g_current.mode || target.w != g_current.w ||
        target.h != g_current.h || target.monitor != g_current.monitor) {
        g_startupState = target;
        g_startupPending = true;
    } else {
        // No re-apply needed, but adopt the persisted request (reqW/reqH may be
        // the 0 "Auto" sentinel) so GetInfoJson reports the Native/Auto intent
        // and the settings page preselects it instead of a concrete resolution.
        g_current = target;
        g_lastGood = target;
    }
}

void Finalize() {
    if (Direct3D_IsFullscreen()) Direct3D_SetFullscreenState(false, nullptr);
    for (auto* o : g_outputs) if (o) o->Release();
    g_outputs.clear();
    g_monitors.clear();
    if (g_adapter) { g_adapter->Release(); g_adapter = nullptr; }
    if (g_factory) { g_factory->Release(); g_factory = nullptr; }
    g_initialized = false;
}

void Update(double dt) {
    if (!g_initialized) return;

    // 0. Startup: apply the persisted mode once (after ShowWindow), no countdown.
    if (g_startupPending) {
        g_startupPending = false;
        g_lastGood = g_current;
        g_current = ApplyDisplayState(g_startupState);   // effective (fallback-aware)
    }

    // 1. Staged risky change → apply live, start the 15s revert countdown.
    if (g_pendingApply) {
        g_pendingApply = false;
        g_lastGood = g_current;
        DisplayState st = g_pendingState;
        ResolveSize(st);
        g_current = ApplyDisplayState(st);
        g_countingDown = true;
        g_revertRemaining = REVERT_SECONDS;
        g_lastPushedSec = -1;
    }

    // 2. Confirm → persist and stop the countdown.
    if (g_pendingConfirm) {
        g_pendingConfirm = false;
        g_countingDown = false;
        PersistCurrent();
        UI::PushDisplayRevertTick(0);   // hide the dialog
    }

    // 3. Revert → restore last good, persist nothing.
    if (g_pendingRevert) {
        g_pendingRevert = false;
        g_countingDown = false;
        g_current = ApplyDisplayState(g_lastGood);
        UI::PushDisplayRevertTick(0);
    }

    // 4. Deferred alt-tab handling for exclusive fullscreen.
    if (g_pendingActivate) {
        g_pendingActivate = false;
        if (g_current.mode == Mode::Exclusive) {
            if (!g_activateState) {
                if (Direct3D_IsFullscreen()) Direct3D_SetFullscreenState(false, nullptr);
            } else if (g_restoreExclusive && !Direct3D_IsFullscreen()) {
                // Re-enter on focus gain; if exclusive is no longer available this
                // falls back to windowed and clears g_restoreExclusive, so it does
                // not re-fire every alt-tab.
                g_current = ApplyDisplayState(g_current);
            }
        }
    }

    // 5. Tick the revert countdown.
    if (g_countingDown) {
        g_revertRemaining -= dt;
        int sec = (int)std::ceil(g_revertRemaining);
        if (sec < 0) sec = 0;
        if (sec != g_lastPushedSec) {
            UI::PushDisplayRevertTick(sec);
            g_lastPushedSec = sec;
        }
        if (g_revertRemaining <= 0.0) {
            g_countingDown = false;
            g_current = ApplyDisplayState(g_lastGood);
            UI::PushDisplayRevertTick(0);
        }
    }
}

std::string GetInfoJson() {
    auto& cfg = Config::GetInstance();
    std::string aspect = cfg.Get("display.aspect_ratio").AsString();
    if (aspect.empty()) aspect = "all";

    std::string s = "{";
    s += "\"current\":{";
    s += "\"mode\":\"";    s += ModeToString(g_current.mode);            s += "\",";
    s += "\"width\":";     s += std::to_string(g_current.reqW);          s += ",";
    s += "\"height\":";    s += std::to_string(g_current.reqH);          s += ",";
    s += "\"monitor\":";   s += std::to_string(g_current.monitor);       s += ",";
    s += "\"vsync\":";     s += (cfg.Get("display.vsync").AsBool() ? "true" : "false"); s += ",";
    s += "\"fov\":";       s += std::to_string((int)cfg.Get("display.fov").AsFloat());  s += ",";
    s += "\"maxFps\":";    s += std::to_string((int)cfg.Get("display.max_fps").AsInt()); s += ",";
    s += "\"aspect\":\"";  s += JsonEscape(aspect);                      s += "\"";
    s += "},";

    s += "\"monitors\":[";
    for (size_t i = 0; i < g_monitors.size(); ++i) {
        const MonInfo& m = g_monitors[i];
        if (i) s += ",";
        s += "{";
        s += "\"index\":";   s += std::to_string((int)i);                s += ",";
        s += "\"name\":\"";  s += JsonEscape(m.deviceName);              s += "\",";
        s += "\"primary\":"; s += (m.primary ? "true" : "false");        s += ",";
        s += "\"desktop\":{\"width\":"; s += std::to_string(m.nativeW);
        s += ",\"height\":";            s += std::to_string(m.nativeH);
        s += ",\"refresh\":";           s += std::to_string(m.nativeHz); s += "},";
        s += "\"modes\":[";
        for (size_t j = 0; j < m.modes.size(); ++j) {
            if (j) s += ",";
            s += "{\"width\":";  s += std::to_string(m.modes[j].first);
            s += ",\"height\":"; s += std::to_string(m.modes[j].second); s += "}";
        }
        s += "]}";
    }
    s += "]}";
    return s;
}

void RequestApply(Mode m, int width, int height, int monitorIdx) {
    g_pendingState.mode    = m;
    g_pendingState.reqW    = width;
    g_pendingState.reqH    = height;
    g_pendingState.monitor = monitorIdx;
    g_pendingApply = true;
}

void RequestConfirm() { g_pendingConfirm = true; }
void RequestRevert()  { g_pendingRevert  = true; }

void OnWindowResize(int w, int h) {
    if (!g_initialized || w <= 0 || h <= 0) return;
    if (w == (int)Direct3D_GetBackBufferWidth() && h == (int)Direct3D_GetBackBufferHeight())
        return;   // idempotent: collapse duplicate WM_SIZE / imperative calls
    Direct3D_Resize((unsigned)w, (unsigned)h);
    UI::Resize(w, h);
    Mouse_RefreshClipRect();
}

void OnActivateApp(bool active) {
    if (!g_initialized) return;
    g_pendingActivate = true;
    g_activateState = active;
}

bool IsInitialized() { return g_initialized; }

}  // namespace Display
