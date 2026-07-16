#pragma once
//=============================================================================
// debug_log.h
//
// File-only diagnostic logger for client-side prediction issues
// (correction-mode behavior, jump-input timing, state mismatch).
//
// Configured by [debug] section in config.toml. No-op when disabled.
//
// Single-threaded — must be called from the main game-loop thread only.
//=============================================================================

#include <string>

void DebugLog_Initialize();
void DebugLog_Finalize();

// Resolved root of the log tree ([log].root), before the per-run timestamp dir:
// CWD-relative in Debug (dev logs stay in the project dir), exe-relative in
// Release (a wrong CWD must not scatter logs). Single source of truth — the UI
// layer parks ultralight.log here too, so all logs land together.
std::string DebugLog_LogsRoot();

bool  DebugLog_IsEnabled();
float DebugLog_GetErrorThreshold();
bool  DebugLog_LogEveryCorrection();
bool  DebugLog_LogJumpEvents();
bool  DebugLog_LogSoftModeState();
int   DebugLog_GetSoftStateIntervalTicks();

// printf-style; line is prefixed with "[HH:MM:SS.mmm][TAG] " and flushed.
// No-op when disabled, so callers may invoke unconditionally.
void DebugLog_Printf(const char* tag, const char* fmt, ...);
