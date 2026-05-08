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

void DebugLog_Initialize();
void DebugLog_Finalize();

bool  DebugLog_IsEnabled();
float DebugLog_GetErrorThreshold();
bool  DebugLog_LogEveryCorrection();
bool  DebugLog_LogJumpEvents();
bool  DebugLog_LogSoftModeState();
int   DebugLog_GetSoftStateIntervalTicks();

// printf-style; line is prefixed with "[HH:MM:SS.mmm][TAG] " and flushed.
// No-op when disabled, so callers may invoke unconditionally.
void DebugLog_Printf(const char* tag, const char* fmt, ...);
