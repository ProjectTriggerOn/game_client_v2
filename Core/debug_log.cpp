#include "debug_log.h"
#include "config.h"
#include "exe_path.h"

#include <cctype>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {
    bool          g_enabled                = false;
    float         g_errorThreshold         = 0.5f;
    bool          g_logEveryCorrection     = false;
    bool          g_logJumpEvents          = true;
    bool          g_logSoftModeState       = true;
    int           g_softStateIntervalTicks = 16;

    std::filesystem::path                              g_runDir;
    std::unordered_map<std::string, std::ofstream>     g_files;
    std::chrono::steady_clock::time_point              g_startTime;

    // Convert tag to a filesystem-safe lowercase stem.
    // Accepts [A-Za-z0-9_]; everything else becomes '_'.
    std::string TagToFileStem(const char* tag)
    {
        std::string out;
        if (!tag) return out;
        for (const char* p = tag; *p; ++p) {
            unsigned char c = static_cast<unsigned char>(*p);
            if (std::isalnum(c) || c == '_') {
                out.push_back(static_cast<char>(std::tolower(c)));
            } else {
                out.push_back('_');
            }
        }
        return out;
    }

    // Build "YYYY-MM-DD_HH-MM-SS" from current local time.
    std::string MakeRunTimestamp()
    {
        auto now   = std::chrono::system_clock::now();
        auto t     = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d_%02d-%02d-%02d",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec);
        return std::string(buf);
    }

    // Look up (or lazily open) the ofstream for `tag`. Returns nullptr on failure.
    std::ofstream* GetOrOpenFile(const char* tag)
    {
        std::string stem = TagToFileStem(tag);
        if (stem.empty()) return nullptr;

        auto it = g_files.find(stem);
        if (it != g_files.end()) {
            return it->second.is_open() ? &it->second : nullptr;
        }

        std::ofstream& ofs = g_files[stem];
        std::filesystem::path path = g_runDir / (stem + ".log");
        ofs.open(path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            return nullptr;
        }

        ofs << "=== TriggerOn log [" << (tag ? tag : "") << "] opened ==="
            << " errorThreshold=" << g_errorThreshold
            << " logEveryCorrection=" << g_logEveryCorrection
            << " logJumpEvents=" << g_logJumpEvents
            << " logSoftModeState=" << g_logSoftModeState
            << " softStateIntervalTicks=" << g_softStateIntervalTicks
            << "\n";
        ofs.flush();
        return &ofs;
    }
}

std::string DebugLog_LogsRoot()
{
    std::string root = Config::GetInstance().GetString("log", "root", "logs");
    std::filesystem::path rootPath(root);
#if !defined(_DEBUG) && !defined(DEBUG)
    // Release: anchor a relative log root to the exe dir so a wrong CWD
    // (shortcut/script launch) can't scatter logs. Debug keeps CWD-relative so
    // dev logs stay in the project dir (mirrors UI/ui_manager.cpp GetUiRoot).
    if (rootPath.is_relative())
        rootPath = std::filesystem::path(ExeDirA()) / rootPath;
#endif
    return rootPath.string();
}

void DebugLog_Initialize()
{
    // [log] — generic log system (path + master switch)
    g_enabled = Config::GetInstance().GetBool("log", "enabled", false);

    // [debug] — gameplay-specific filters (untouched semantics)
    g_errorThreshold         = static_cast<float>(Config::GetInstance().GetDouble("debug", "error_threshold", 0.5));
    g_logEveryCorrection     = Config::GetInstance().GetBool("debug", "log_every_correction",  false);
    g_logJumpEvents          = Config::GetInstance().GetBool("debug", "log_jump_events",       true);
    g_logSoftModeState       = Config::GetInstance().GetBool("debug", "log_softmode_state",    true);
    g_softStateIntervalTicks = Config::GetInstance().GetInt ("debug", "softmode_sample_ticks", 16);
    if (g_softStateIntervalTicks <= 0) g_softStateIntervalTicks = 1;

    if (!g_enabled) return;

    std::filesystem::path runDir = std::filesystem::path(DebugLog_LogsRoot()) / MakeRunTimestamp();

    std::error_code ec;
    std::filesystem::create_directories(runDir, ec);
    if (ec) {
        g_enabled = false;
        return;
    }

    g_runDir    = runDir;
    g_startTime = std::chrono::steady_clock::now();
}

void DebugLog_Finalize()
{
    for (auto& kv : g_files) {
        if (kv.second.is_open()) {
            kv.second << "=== TriggerOn log [" << kv.first << "] closed ===\n";
            kv.second.close();
        }
    }
    g_files.clear();
    g_enabled = false;
}

bool  DebugLog_IsEnabled()                 { return g_enabled; }
float DebugLog_GetErrorThreshold()         { return g_errorThreshold; }
bool  DebugLog_LogEveryCorrection()        { return g_logEveryCorrection; }
bool  DebugLog_LogJumpEvents()             { return g_logJumpEvents; }
bool  DebugLog_LogSoftModeState()          { return g_logSoftModeState; }
int   DebugLog_GetSoftStateIntervalTicks() { return g_softStateIntervalTicks; }

void DebugLog_Printf(const char* tag, const char* fmt, ...)
{
    if (!g_enabled) return;

    std::ofstream* ofs = GetOrOpenFile(tag);
    if (!ofs) return;

    auto now = std::chrono::steady_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime).count();
    long long sec      = ms / 1000;
    int        millis  = static_cast<int>(ms % 1000);
    int        hours   = static_cast<int>(sec / 3600);
    int        minutes = static_cast<int>((sec % 3600) / 60);
    int        seconds = static_cast<int>(sec % 60);

    char header[64];
    std::snprintf(header, sizeof(header), "[%02d:%02d:%02d.%03d][%s] ",
                  hours, minutes, seconds, millis, tag ? tag : "");

    char body[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    *ofs << header << body << "\n";
    ofs->flush();
}
