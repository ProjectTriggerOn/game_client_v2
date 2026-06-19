#pragma once
#include <Ultralight/platform/FileSystem.h>
#include <string>

namespace UI {

// Loads UI assets from the given root directory:
//   Debug:   <exe_dir>/../../ui_src   (reads sources directly; enables Slice F hot reload)
//   Release: <exe_dir>/resource/ui    (post-build mirrored output)
class UIFileSystem : public ultralight::FileSystem {
public:
    explicit UIFileSystem(const std::string& root);
    ~UIFileSystem() override = default;

    bool                FileExists(const ultralight::String& path) override;
    ultralight::String  GetFileMimeType(const ultralight::String& path) override;
    ultralight::String  GetFileCharset(const ultralight::String& path) override;
    ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String& path) override;

private:
    // Resolve a path from Ultralight (e.g. "index.html" or "/pages/title.html") to an absolute path
    std::string Resolve(const ultralight::String& path) const;

    std::string m_root;  // normalized, no trailing separator
};

}  // namespace UI
