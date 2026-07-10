//=============================================================================
// model_catalog.cpp
//=============================================================================
#ifdef EDITOR_ENABLED
#include "model_catalog.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>

namespace {
    std::vector<std::string>                 g_Names;    // "resource/model/foo.fbx"
    std::unordered_map<std::string, MODEL*>  g_Cache;
    std::string                              g_Dir;
}

void ModelCatalog_Init(const char* dir) {
    g_Names.clear();
    g_Dir = dir ? dir : "resource/model";
    std::string pattern = g_Dir + "\\*.fbx";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                g_Names.push_back(g_Dir + "/" + fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
}

int         ModelCatalog_Count()      { return (int)g_Names.size(); }
const char* ModelCatalog_Name(int i)  { return (i >= 0 && i < (int)g_Names.size()) ? g_Names[i].c_str() : ""; }

MODEL* ModelCatalog_Get(const char* assetName) {
    if (!assetName || !assetName[0]) return nullptr;
    std::string key = assetName;
    auto it = g_Cache.find(key);
    if (it != g_Cache.end()) return it->second;
    MODEL* m = ModelLoad(key.c_str(), 1.0f);   // nullptr on failure is cached to avoid re-tries
    g_Cache[key] = m;
    return m;
}

void ModelCatalog_Finalize() {
    for (auto& kv : g_Cache) if (kv.second) ModelRelease(kv.second);
    g_Cache.clear();
    g_Names.clear();
}

#endif // EDITOR_ENABLED
