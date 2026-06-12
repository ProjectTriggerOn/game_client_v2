#include "ui_bridge.h"
#include "ui_manager.h"
#include "config.h"
#include "scene.h"

#include <Ultralight/View.h>
#include <AppCore/JSHelpers.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace {

using namespace ultralight;

std::string JSValueToStdString(const JSValue& v) {
    String s = ((JSValue&)v).ToString();
    return std::string(s.utf8().data());
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

    auto ctx = view->LockJSContext();
    SetJSContext(*ctx);

    // Create (or reuse) window.game, then attach the API onto it
    JSValue gameVal = JSEval("window.game = window.game || {}; window.game");
    JSObject game = gameVal.ToObject();

    // --- state / app control -------------------------------------------------

    game["setState"] = (JSCallback)[](const JSObject&, const JSArgs& args) {
        if (args.empty()) return;
        const std::string name = JSValueToStdString(args[0]);
        DebugLog("[UI:bridge] game.setState ", name);
        // Slice D: echo straight back to the Router (deferred to avoid JS
        // re-entrancy). Slice E replaces this with real GameState transitions
        // that drive Router via Push::State.
        UI::ShowPageDeferred(name.c_str());
    };

    game["quit"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.quit", "");
        PostQuitMessage(0);
    };

    game["startLocalGame"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.startLocalGame", "");
        // Scene transition applies at end of frame (Scene_Refresh); the HUD page
        // switch is deferred to next Render. TODO(Slice E): full GameState wiring.
        Scene_Change(SCENE_GAME);
        UI::SetInteractiveLevel(UI::InteractiveLevel::Display);
        UI::ShowPageDeferred("hud");
    };

    game["returnToTitle"] = (JSCallback)[](const JSObject&, const JSArgs&) {
        DebugLog("[UI:bridge] game.returnToTitle", "");
        // Returns to the UI sandbox scene for now (the real title scene flow
        // arrives with Slice E). TODO(Slice E): network teardown.
        Scene_Change(SCENE_UI_TEST);
        UI::SetInteractiveLevel(UI::InteractiveLevel::Interactive);
        UI::ShowPageDeferred("title");
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

    // --- misc -----------------------------------------------------------------

    game["getPlayerList"] = (JSCallbackWithRetval)[](const JSObject&, const JSArgs&) -> JSValue {
        // TODO(lobby): fill from server snapshot
        return JSEval("[]");
    };

    game["getVersion"] = (JSCallbackWithRetval)[](const JSObject&, const JSArgs&) -> JSValue {
        return JSValue(__DATE__ " " __TIME__);
    };

    game["log"] = (JSCallback)[](const JSObject&, const JSArgs& args) {
        if (args.empty()) return;
        DebugLog("[UI:js] ", JSValueToStdString(args[0]));
    };

    DebugLog("[UI:bridge] game.* registered", "");
}

}  // namespace Bridge
}  // namespace UI
