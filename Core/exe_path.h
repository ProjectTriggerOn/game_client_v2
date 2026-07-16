#pragma once
//=============================================================================
// exe_path.h
//
// Absolute directory of the running executable. Resolve files (config, logs,
// DLL search path) exe-relative rather than CWD-relative so launching the
// shipped build from a wrong working directory — a Start-menu/desktop shortcut
// with a blank "Start in", or a script that chdir's elsewhere — cannot send a
// file load to the wrong place. main.cpp resolves config\config.toml exe-first
// and falls back to the CWD-relative path for the Debug/dev workflow (run from
// game_client/, where config/ lives); a truly missing file is not fatal —
// Config::Load falls back to built-in defaults (see main.cpp, Core/config.h).
// ExeDirW() additionally feeds AddDllDirectory (the exe-relative bin/ holding
// the Ultralight + assimp DLLs) and the Release-only SetCurrentDirectoryW.
// Mirrors the exe-relative resolution the UI layer already does for Ultralight
// (UI/ui_manager.cpp GetExeDirA / GetUiRoot).
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

// Wide-char twin of ExeDirA — AddDllDirectory and friends take PCWSTR only.
inline std::wstring ExeDirW()
{
    wchar_t buf[MAX_PATH];
    DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf, (n > 0 && n < MAX_PATH) ? n : 0u);
    size_t slash = s.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? std::wstring() : s.substr(0, slash + 1);
}
