#pragma once
#include <Ultralight/platform/FileSystem.h>
#include <string>

namespace UI {

// 从指定根目录加载 UI 资源：
//   Debug:   <exe_dir>/../../ui_src   （直接读源，支持后续 Slice F 热重载）
//   Release: <exe_dir>/resource/ui    （post-build 镜像产物）
class UIFileSystem : public ultralight::FileSystem {
public:
    explicit UIFileSystem(const std::string& root);
    ~UIFileSystem() override = default;

    bool                FileExists(const ultralight::String& path) override;
    ultralight::String  GetFileMimeType(const ultralight::String& path) override;
    ultralight::String  GetFileCharset(const ultralight::String& path) override;
    ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String& path) override;

private:
    // 把 Ultralight 给的 path (e.g. "index.html" 或 "/pages/title.html") 解析为绝对路径
    std::string Resolve(const ultralight::String& path) const;

    std::string m_root;  // 已规范化，末尾不带分隔符
};

}  // namespace UI
