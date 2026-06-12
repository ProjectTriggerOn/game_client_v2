#include "ui_filesystem.h"
#include <Ultralight/Buffer.h>

#include <fstream>
#include <vector>
#include <cctype>
#include <algorithm>

namespace {

// Extract lowercase extension including the dot (".html" / ".js" / "")
std::string GetExtLower(const std::string& path) {
    auto dot = path.find_last_of('.');
    auto slash = path.find_last_of("/\\");
    if (dot == std::string::npos) return "";
    if (slash != std::string::npos && dot < slash) return "";  // dot belongs to a directory name
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return ext;
}

const char* MimeFromExt(const std::string& ext) {
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".js")                    return "application/javascript";
    if (ext == ".css")                   return "text/css";
    if (ext == ".json")                  return "application/json";
    if (ext == ".png")                   return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")                   return "image/gif";
    if (ext == ".svg")                   return "image/svg+xml";
    if (ext == ".webp")                  return "image/webp";
    if (ext == ".ttf")                   return "font/ttf";
    if (ext == ".otf")                   return "font/otf";
    if (ext == ".woff")                  return "font/woff";
    if (ext == ".woff2")                 return "font/woff2";
    if (ext == ".txt")                   return "text/plain";
    return "application/unknown";
}

// Strip trailing path separators
std::string NormalizeRoot(std::string r) {
    while (!r.empty() && (r.back() == '/' || r.back() == '\\')) r.pop_back();
    return r;
}

}  // namespace

namespace UI {

UIFileSystem::UIFileSystem(const std::string& root) : m_root(NormalizeRoot(root)) {}

std::string UIFileSystem::Resolve(const ultralight::String& path) const {
    std::string p = path.utf8().data();

    // Ultralight's internal assets (cacert.pem / icudt67l.dat) arrive as absolute
    // paths built from Config::resource_path_prefix; UI assets (index.html etc.)
    // arrive as relative paths. Pass absolute paths through; only join relative
    // ones onto m_root.
    auto isWindowsAbsolute = [](const std::string& s) {
        if (s.size() >= 3 && s[1] == ':' &&
            (s[2] == '\\' || s[2] == '/') &&
            std::isalpha((unsigned char)s[0])) return true;            // C:\... or C:/...
        if (s.size() >= 2 && (s[0] == '\\' && s[1] == '\\')) return true; // \\server\share\...
        return false;
    };

    if (isWindowsAbsolute(p)) return p;

    // Relative: strip any leading separators, then join under m_root
    while (!p.empty() && (p.front() == '/' || p.front() == '\\')) p.erase(p.begin());
    return m_root + "\\" + p;
}

bool UIFileSystem::FileExists(const ultralight::String& path) {
    std::ifstream ifs(Resolve(path), std::ios::binary);
    return ifs.good();
}

ultralight::String UIFileSystem::GetFileMimeType(const ultralight::String& path) {
    return MimeFromExt(GetExtLower(path.utf8().data()));
}

ultralight::String UIFileSystem::GetFileCharset(const ultralight::String& /*path*/) {
    return "utf-8";
}

ultralight::RefPtr<ultralight::Buffer> UIFileSystem::OpenFile(const ultralight::String& path) {
    const std::string abs = Resolve(path);
    std::ifstream ifs(abs, std::ios::binary | std::ios::ate);
    if (!ifs) return nullptr;

    auto sz = ifs.tellg();
    if (sz < 0) return nullptr;
    ifs.seekg(0, std::ios::beg);

    std::vector<char> buf((size_t)sz);
    if (sz > 0 && !ifs.read(buf.data(), sz)) return nullptr;

    // CreateFromCopy allocates aligned memory and memcpys; simplest and safest
    return ultralight::Buffer::CreateFromCopy(buf.data(), buf.size());
}

}  // namespace UI
