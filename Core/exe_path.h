#pragma once
//=============================================================================
// exe_path.h
//
// Absolute directory of the running executable. Resolve files (config, logs)
// exe-relative rather than CWD-relative so launching the shipped build from a
// wrong working directory — a Start-menu/desktop shortcut with a blank
// "Start in", or a script that chdir's elsewhere — cannot send a mandatory
// file load to the wrong place (config.toml missing => hard crash before any
// window; see Core/config.h). Mirrors the exe-relative resolution the UI layer
// already does for Ultralight (UI/ui_manager.cpp GetExeDirA / GetUiRoot).
//=============================================================================

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Directory of the running exe, WITH a trailing backslash (so callers can just
// append a filename). Empty string only if the OS query fails (never expected).
inline std::string ExeDirA()
{
    char buf[MAX_PATH];
    DWORD n = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(buf, (n > 0 && n < MAX_PATH) ? n : 0u);
    size_t slash = s.find_last_of("\\/");
    return (slash == std::string::npos) ? std::string() : s.substr(0, slash + 1);
}
