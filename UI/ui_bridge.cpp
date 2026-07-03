#include "ui_bridge.h"
#include "ui_manager.h"
#include "ui_policy.h"
#include "config.h"
#include "scene.h"
#include "game.h"
#include "display_manager.h"

#include <Ultralight/View.h>
#include <AppCore/JSHelpers.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace {

using namespace ultralight;

// View whose JS context the push helpers target. Set in Register (OnDOMReady),
// so it always points at the current navigation's context.
ultralight::View* g_bridgeView = nullptr;

// Call window.<fnName>(a, b) on the current JS context if it exists.
// Fresh property lookup each call (cheap hash lookup; avoids stale JSFunction
// refs across navigation and avoids building arg strings). No-op if the handler
// isn't defined yet (e.g. HUD page not loaded).
void CallJsFn2(const char* fnName, double a, double b) {
    if (!g_bridgeView) return;
    auto ctx = g_bridgeView->LockJSContext();
    SetJSContext(*ctx);
    JSObject global = JSGlobalObject();
    if (!global.HasProperty(fnName)) return;   // handler not defined yet — silent no-op

    // IMPORTANT: keep the JSPropertyValue type (use auto). operator[] returns a
    // JSPropertyValue whose value is resolved lazily via a virtual instance().
    // Slicing it into a plain `JSValue` (e.g. `JSValue v = global[fnName];`)
    // copies the still-null base instance_, so IsFunction()/ToFunction() then see
    // an empty value — the bug that left the HUD frozen ("not a function").
    auto prop = global[fnName];
    if (!prop.IsFunction()) return;

    JSFunction fn = prop.ToFunction();
    if (!fn.IsValid()) return;

    JSArgs args;
    args.push_back(JSValue(a));
    args.push_back(JSValue(b));
    fn(args);
}

// Variants of CallJsFn2 with different arities. Same fresh-lookup contract and
// the same keep-the-JSPropertyValue-via-auto rule (see the note above).
void CallJsFn1(const char* fnName, double a) {
    if (!g_bridgeView) return;
    auto ctx = g_bridgeView->LockJSContext();
    SetJSContext(*ctx);
    JSObject global = JSGlobalObject();
    if (!global.HasProperty(fnName)) return;
    auto prop = global[fnName];
    if (!prop.IsFunction()) return;
    JSFunction fn = prop.ToFunction();
    if (!fn.IsValid()) return;
    JSArgs args;
    args.push_back(JSValue(a));
    fn(args);
}

void CallJsFn4(const char* fnName, double a, double b, double c, double d) {
    if (!g_bridgeView) return;
    auto ctx = g_bridgeView->LockJSContext();
    SetJSContext(*ctx);
    JSObject global = JSGlobalObject();
    if (!global.HasProperty(fnName)) return;
    auto prop = global[fnName];
    if (!prop.IsFunction()) return;
    JSFunction fn = prop.ToFunction();
    if (!fn.IsValid()) return;
    JSArgs args;
    args.push_back(JSValue(a));
    args.push_back(JSValue(b));
    args.push_back(JSValue(c));
    args.push_back(JSValue(d));
    fn(args);
}

// Pass a single string argument (e.g. a JSON payload the handler JSON.parses).
void CallJsFnStr(const char* fnName, const char* s) {
    if (!g_bridgeView || !s) return;
    auto ctx = g_bridgeView->LockJSContext();
    SetJSContext(*ctx);
    JSObject global = JSGlobalObject();
    if (!global.HasProperty(fnName)) return;
    auto prop = global[fnName];
    if (!prop.IsFunction()) return;
    JSFunction fn = prop.ToFunction();
    if (!fn.IsValid()) return;
    JSArgs args;
    args.push_back(JSValue(String(s)));
    fn(args);
}

std::string JSValueToStdString(const JSValue& v) {
    String s = ((JSValue&)v).ToString();
    return std::string(s.utf8().data());
}

// JS number → int. Bind to a named lvalue first (the JSHelpers accessors are
// non-const); casting args[i] directly would trip C4238 (rvalue used as lvalue).
int JSValueToInt(const JSValue& v) {
    return (int)((JSValue&)v).ToNumber();
}

// JS number/bool/string → ConfigValue (Config::Set preserves integer-ness of
// existing TOML values, so passing whole floats for ints is fine)
ConfigValue JSValueToConfigValue(const JSValue& v) {
    JSValue& mv = (JSValue&)v;   // JSHelpers accessors are non-const
    if (mv.IsBoolean()) return ConfigValue(mv.ToBoolean());
    if (mv.IsNumber())  return ConfigValue(mv.ToNumber());
    if (mv.IsString())  return ConfigValue(JSValueToStdString(v));
    return {};
}

void DebugLog(const char* prefix, const std::string& text) {
    std::string line = prefix;
    line += text;
    line += "\n";
    OutputDebugStringA(line.c_str());
}

}  // namespace

namespace UI {
namespace Bridge {

void Register(ultralight::View* view) {
    if (!view) return;
    g_bridgeView = view;

    auto ctx = view->LockJSContext();
    SetJSContext(*ctx);

    // Create (or reuse) window.game, then attach the API onto it
    JSValue gameVal = JSEval("window.game = window.game || {}; window.game");
    JSObject game = gameVal.ToObject();

    // --- state / app control -------------------------------------------------
    //
    // Menu intent verbs change GameState only; the active page + input level are
    // derived from GameState by UIPolicy_Apply (ui_policy.h). The menus never
    // touch the Router or InteractiveLevel directly.

    game["resume"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.resume", "");
        Game_SetState(PLAY);
    };

    game["openSettings"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.openSettings", "");
        Game_SetState(SETTING);
    };

    game["backToPause"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.backToPause", "");
        Game_SetState(PAUSE);
    };

    // Sandbox-only page preview (used by the ui_test title page). Does NOT drive
    // GameState — in SCENE_GAME the policy would override it next frame anyway.
    game["setState"] = (JSCallback)[](const JSObject&, const JSArgs& args) {
        if (args.empty()) return;
        const std::string name = JSValueToStdString(args[0]);
        DebugLog("[UI:bridge] game.setState (sandbox preview) ", name);
        UI::ShowPageDeferred(name.c_str());
    };

    game["quit"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.quit", "");
        PostQuitMessage(0);
    };

    game["startLocalGame"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.startLocalGame", "");
        // Masked transition: the loading curtain covers the screen, the swap to
        // SCENE_GAME (and its heavy Game_Initialize) happens while black, then the
        // game is revealed. Game_Initialize sets GameState=PLAY and re-arms the
        // mock session; UIPolicy_Apply derives the HUD page + level.
        SceneTransition_To(SCENE_GAME);
    };

    game["returnToTitle"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.returnToTitle", "");
        // Same masked transition back to the title; UIPolicy_Apply restores the
        // title page + Interactive once revealed.
        // TODO(lobby): network teardown when leaving a live game.
        SceneTransition_To(SCENE_TITLE);
    };

    // --- config ---------------------------------------------------------------

    game["getConfig"] = (JSCallbackWithRetval)[](const JSObject&, const JSArgs& args) -> JSValue {
        if (args.empty()) return JSValue(JSValueNullTag{});
        const std::string key = JSValueToStdString(args[0]);
        const ConfigValue v = Config::GetInstance().Get(key);
        switch (v.type) {
        case ConfigValue::Type::Bool:   return JSValue(v.b);
        case ConfigValue::Type::Int:    return JSValue((double)v.i);
        case ConfigValue::Type::Float:  return JSValue(v.f);
        case ConfigValue::Type::String: return JSValue(v.s.c_str());
        default:                        return JSValue(JSValueNullTag{});
        }
    };

    game["setConfig"] = (JSCallback)[](const JSObject&, const JSArgs& args) {
        if (args.size() < 2) return;
        const std::string key = JSValueToStdString(args[0]);
        const ConfigValue val = JSValueToConfigValue(args[1]);
        if (val.IsNone()) return;
        Config::GetInstance().Set(key, val);   // fires subscribers (realtime keys apply now)
    };

    game["saveConfig"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        const bool ok = Config::GetInstance().SaveToFile();
        DebugLog("[UI:bridge] game.saveConfig ", ok ? "ok" : "FAILED");
    };

    // --- display settings -----------------------------------------------------
    //
    // getDisplayInfo returns cached DXGI data (monitors + modes + current
    // settings) as a JSON string the page JSON.parses — read-only, so it's safe
    // synchronously here. apply/confirm/revert only STAGE a request; the actual
    // window + swap-chain change runs in Display::Update on the main loop (poking
    // the window inside a JS callback would pump messages and re-enter WndProc).

    game["getDisplayInfo"] = (JSCallbackWithRetval)[](const JSObject&, const JSArgs&) -> JSValue {
        const std::string json = Display::GetInfoJson();
        return JSValue(json.c_str());   // JS side JSON.parses it
    };

    game["applyDisplaySettings"] = (JSCallback)[](const JSObject&, const JSArgs& args) {
        if (args.size() < 4) return;
        const std::string m = JSValueToStdString(args[0]);
        Display::Mode mode = (m == "borderless") ? Display::Mode::Borderless
                           : (m == "exclusive")  ? Display::Mode::Exclusive
                                                 : Display::Mode::Windowed;
        const int w   = JSValueToInt(args[1]);
        const int h   = JSValueToInt(args[2]);
        const int mon = JSValueToInt(args[3]);
        DebugLog("[UI:bridge] game.applyDisplaySettings ", m);
        Display::RequestApply(mode, w, h, mon);   // staged; executed in Display::Update
    };

    game["confirmDisplaySettings"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.confirmDisplaySettings", "");
        Display::RequestConfirm();
    };

    game["revertDisplaySettings"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.revertDisplaySettings", "");
        Display::RequestRevert();
    };

    // --- misc -----------------------------------------------------------------

    game["getPlayerList"] = (JSCallbackWithRetval)[](const JSObject&, const JSArgs&) -> JSValue {
        // TODO(lobby): fill from server snapshot
        return JSEval("[]");
    };

    game["getVersion"] = (JSCallbackWithRetval)[](const JSObject&, const JSArgs&) -> JSValue {
        return JSValue(__DATE__ " " __TIME__);
    };

    // Boot/reload page authority (Slice F). router.js calls this at startup —
    // including after a hot-reload View::Reload — to land on the page that matches
    // the current (scene, GameState) rather than always 'title'. UIPolicy's static
    // transition trackers survive a reload, so without this a reload would leave
    // the view stuck on the boot default (docs §10).
    game["getBootPage"] = (JSCallbackWithRetval)[](const JSObject&, const JSArgs&) -> JSValue {
        return JSValue(UIPolicy_DerivePage());
    };

    game["log"] = (JSCallback)[](const JSObject&, const JSArgs& args) {
        if (args.empty()) return;
        DebugLog("[UI:js] ", JSValueToStdString(args[0]));
    };

    DebugLog("[UI:bridge] game.* registered", "");
}

void Unbind() {
    g_bridgeView = nullptr;
}

// --- C++ → JS push (docs §8.2) ----------------------------------------------

void PushHealth(int current, int maxHp) {
    CallJsFn2("onHealthChanged", (double)current, (double)maxHp);
}

void PushAmmo(int current, int reserve) {
    CallJsFn2("onAmmoChanged", (double)current, (double)reserve);
}

void PushScores(int red, int blue) {
    CallJsFn2("onScoresChanged", (double)red, (double)blue);
}

void PushMatchTimer(float secondsRemaining) {
    CallJsFn1("onMatchTimerChanged", (double)secondsRemaining);
}

void PushKillFeed(int killerId, int victimId, int killerTeam, int victimTeam) {
    CallJsFn4("onKillFeed", (double)killerId, (double)victimId,
              (double)killerTeam, (double)victimTeam);
}

void PushScoreboard(const char* json) {
    CallJsFnStr("onScoreboardData", json);
}

void PushScoreboardVisible(bool visible) {
    CallJsFn1("onScoreboardVisible", visible ? 1.0 : 0.0);
}

void PushMatchResult(const char* json) {
    CallJsFnStr("onMatchResult", json);
}

void PushDisplayRevertTick(int secondsLeft) {
    CallJsFn1("onDisplayRevertTick", (double)secondsLeft);
}

}  // namespace Bridge
}  // namespace UI
