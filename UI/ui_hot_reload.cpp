#include "ui_hot_reload.h"

#if defined(_DEBUG) || defined(DEBUG)

#include "ui_manager.h"   // UI::ReloadView / UI::ReloadStyles (view-owning helpers)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <cwctype>

namespace {

// Worker → main-thread handoff. The mutex-guarded vector is plenty: file events
// arrive at well under 10 Hz, so a lock-free queue would only add complexity and
// failure modes for no measurable win (docs §10.1). The main thread swaps the
// whole buffer out each Poll, so the lock is held only for the swap.
std::mutex                g_mutex;
std::vector<std::wstring> g_pending;     // changed paths, relative to watch dir

std::thread        g_thread;
std::atomic<bool>  g_running{false};
HANDLE             g_dir       = INVALID_HANDLE_VALUE;
HANDLE             g_stopEvent = nullptr;

std::wstring Widen(const char* s) {
    if (!s || !*s) return std::wstring();
    int n = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_ACP, 0, s, -1, &w[0], n);
    return w;
}

// Worker-side: convert the relative path to a forward-slash UTF-8 string so it
// matches the file:// stylesheet href in reloadCSS (which uses '/').
std::string ToHrefPath(const std::wstring& w) {
    std::wstring fwd = w;
    for (auto& c : fwd) if (c == L'\\') c = L'/';
    int n = WideCharToMultiByte(CP_UTF8, 0, fwd.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, fwd.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

void WorkerThread(std::wstring watchDir) {
    g_dir = CreateFileW(
        watchDir.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (g_dir == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("[UI:hot] CreateFileW(watchDir) failed — hot reload disabled\n");
        return;
    }

    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE /*manual reset*/, FALSE, nullptr);

    // 64 KB is the documented max for ReadDirectoryChangesW over a network share;
    // far more than enough for a local UI dir's burst of save events.
    std::vector<BYTE> buffer(64 * 1024);

    while (g_running.load(std::memory_order_relaxed)) {
        DWORD bytes = 0;
        BOOL ok = ReadDirectoryChangesW(
            g_dir, buffer.data(), (DWORD)buffer.size(), TRUE /*recursive*/,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_SIZE,
            nullptr, &ov, nullptr);
        if (!ok) break;

        HANDLE waits[2] = { ov.hEvent, g_stopEvent };
        DWORD w = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (w != WAIT_OBJECT_0) {          // stop event, or wait failure
            CancelIoEx(g_dir, &ov);
            break;
        }

        if (!GetOverlappedResult(g_dir, &ov, &bytes, FALSE) || bytes == 0) {
            ResetEvent(ov.hEvent);
            continue;                      // buffer overflow / spurious — re-arm
        }
        ResetEvent(ov.hEvent);

        BYTE* p = buffer.data();
        for (;;) {
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(p);
            std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
            for (auto& c : name) c = (wchar_t)towlower(c);
            {
                std::lock_guard<std::mutex> lk(g_mutex);
                g_pending.push_back(std::move(name));
            }
            if (info->NextEntryOffset == 0) break;
            p += info->NextEntryOffset;
        }
    }

    if (ov.hEvent) CloseHandle(ov.hEvent);
    if (g_dir != INVALID_HANDLE_VALUE) { CloseHandle(g_dir); g_dir = INVALID_HANDLE_VALUE; }
}

}  // namespace

namespace UI {
namespace HotReload {

void Start(const char* watchDir) {
    if (g_running.load()) return;
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_running.store(true);
    g_thread = std::thread(WorkerThread, Widen(watchDir));
    OutputDebugStringA("[UI:hot] watcher started\n");
}

void Stop() {
    if (!g_running.load()) return;
    g_running.store(false);
    if (g_stopEvent) SetEvent(g_stopEvent);     // wake the worker out of its wait
    if (g_thread.joinable()) g_thread.join();
    if (g_stopEvent) { CloseHandle(g_stopEvent); g_stopEvent = nullptr; }
    OutputDebugStringA("[UI:hot] watcher stopped\n");
}

void Poll() {
    std::vector<std::wstring> batch;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_pending.empty()) return;
        batch.swap(g_pending);
    }

    // Debounce: editors emit several events per save (temp file rename + write).
    // Collapse repeats of the same path within a short window. A single global
    // cooldown is too coarse (would drop a CSS edit that lands right after a JS
    // edit), so debounce per path.
    const ULONGLONG now = GetTickCount64();
    static std::unordered_map<std::wstring, ULONGLONG> s_lastFire;
    constexpr ULONGLONG DEBOUNCE_MS = 500;

    bool                     needFullReload = false;
    std::vector<std::string> cssToReload;

    for (auto& path : batch) {
        auto it = s_lastFire.find(path);
        if (it != s_lastFire.end() && now - it->second < DEBOUNCE_MS) continue;
        s_lastFire[path] = now;

        const size_t dot = path.find_last_of(L'.');
        if (dot == std::wstring::npos) continue;
        const std::wstring ext = path.substr(dot + 1);

        if (ext == L"css") {
            cssToReload.push_back(ToHrefPath(path));
        } else if (ext == L"html" || ext == L"js") {
            needFullReload = true;          // structure/logic changed → full reload
        }
        // other extensions (images, fonts, .log) ignored
    }

    // A full View::Reload re-fetches CSS too, so it subsumes any CSS change in the
    // same batch — do it and skip the per-stylesheet path.
    if (needFullReload) {
        OutputDebugStringA("[UI:hot] full reload (html/js changed)\n");
        UI::ReloadView();
        return;
    }

    for (const auto& css : cssToReload) {
        OutputDebugStringA(("[UI:hot] reloadCSS " + css + "\n").c_str());
        UI::ReloadStyles(css.c_str());
    }
}

}  // namespace HotReload
}  // namespace UI

#else  // Release — every entry point is an empty stub

namespace UI {
namespace HotReload {
void Start(const char*) {}
void Stop() {}
void Poll() {}
}  // namespace HotReload
}  // namespace UI

#endif
