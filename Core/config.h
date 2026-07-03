#pragma once
//=============================================================================
// config.h
//
// Global configuration singleton using toml++.
// Header-only — include this file and call Config::GetInstance().Load(path).
//
// Usage:
//   Config::GetInstance().Load("../config.toml");
//   auto host = Config::GetInstance().ServerHost();
//   int  port = Config::GetInstance().ServerPort();
//=============================================================================

#include <string>
#include <stdexcept>
#include <functional>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <filesystem>

// toml++ requires this define for header-only mode
#define TOML_HEADER_ONLY 1
#include "../ThirdParty/toml++/toml.hpp"

//-----------------------------------------------------------------------------
// Small variant for dotted-key access (docs/ultralight_integration.md §9).
// Carries one of: none / bool / int / float / string.
//-----------------------------------------------------------------------------
struct ConfigValue
{
    enum class Type { None, Bool, Int, Float, String };

    Type        type = Type::None;
    bool        b    = false;
    int64_t     i    = 0;
    double      f    = 0.0;
    std::string s;

    ConfigValue() = default;
    ConfigValue(bool v)               : type(Type::Bool),   b(v) {}
    ConfigValue(int v)                : type(Type::Int),    i(v) {}
    ConfigValue(int64_t v)            : type(Type::Int),    i(v) {}
    ConfigValue(double v)             : type(Type::Float),  f(v) {}
    ConfigValue(const char* v)        : type(Type::String), s(v) {}
    ConfigValue(const std::string& v) : type(Type::String), s(v) {}

    bool IsNone() const { return type == Type::None; }

    bool AsBool() const {
        switch (type) {
        case Type::Bool:  return b;
        case Type::Int:   return i != 0;
        case Type::Float: return f != 0.0;
        default:          return false;
        }
    }
    int64_t AsInt() const {
        switch (type) {
        case Type::Int:   return i;
        case Type::Float: return (int64_t)f;
        case Type::Bool:  return b ? 1 : 0;
        default:          return 0;
        }
    }
    double AsFloat() const {
        switch (type) {
        case Type::Float: return f;
        case Type::Int:   return (double)i;
        case Type::Bool:  return b ? 1.0 : 0.0;
        default:          return 0.0;
        }
    }
    const std::string& AsString() const { return s; }
};

class Config
{
public:
    //-------------------------------------------------------------------------
    // Singleton access (Meyer's singleton — thread-safe in C++11 and later)
    //-------------------------------------------------------------------------
    static Config& GetInstance()
    {
        static Config instance;
        return instance;
    }

    //-------------------------------------------------------------------------
    // Load configuration from a TOML file.
    // Call once at startup before accessing any values.
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    // Two-layer config (docs §9.3):
    //   defaults  — hand-written config.toml (comments preserved; never
    //               machine-written; committed to git)
    //   overrides — user_settings.toml (machine-written by SaveToFile; only
    //               contains keys the user changed; gitignored)
    // The user layer is merged over defaults at load; SaveToFile serializes
    // only the user layer, so config.toml is never touched by the game.
    //-------------------------------------------------------------------------
    void Load(const std::string& filePath, const std::string& userFilePath = "user_settings.toml")
    {
        m_Table = toml::parse_file(filePath);
        m_FilePath = filePath;
        m_UserFilePath = userFilePath;

        if (!userFilePath.empty() && std::filesystem::exists(userFilePath)) {
            try {
                m_UserTable = toml::parse_file(userFilePath);
                MergeInto(m_Table, m_UserTable);
            } catch (...) {
                // Corrupt user settings: ignore and run on defaults; the next
                // SaveToFile rewrites the file from scratch.
                m_UserTable = toml::table{};
            }
        }
        m_Loaded = true;
    }

    bool IsLoaded() const { return m_Loaded; }

    //-------------------------------------------------------------------------
    // Dotted-key access + change subscription (docs §9).
    //
    //   Config::GetInstance().Get("input.sensitivity")          → ConfigValue
    //   Config::GetInstance().Set("input.sensitivity", 0.004);  // fires handlers
    //   Config::GetInstance().Subscribe("input.sensitivity", h); // h fired
    //       immediately if the key already has a value
    //
    // Subscribers keep Config decoupled from game modules: each module
    // registers for the keys it owns (observer pattern, docs §9.2).
    //-------------------------------------------------------------------------
    using Handler = std::function<void(const ConfigValue&)>;

    ConfigValue Get(const std::string& dottedKey) const
    {
        const toml::node* node = WalkToNode(dottedKey);
        if (!node) return {};
        // Dispatch on the node's EXACT TOML type. value<bool>() coerces integers
        // (1920 -> true), so probing it first misreads every integer key as 0/1 —
        // guard each probe with is_*() (same predicates MergeInto uses below).
        if (node->is_boolean())        { if (auto v = node->value<bool>())        return ConfigValue(*v); }
        if (node->is_integer())        { if (auto v = node->value<int64_t>())     return ConfigValue(*v); }
        if (node->is_floating_point()) { if (auto v = node->value<double>())      return ConfigValue(*v); }
        if (node->is_string())         { if (auto v = node->value<std::string>()) return ConfigValue(*v); }
        return {};
    }

    void Set(const std::string& dottedKey, const ConfigValue& value)
    {
        // Preserve the existing TOML type where it makes sense: if the stored
        // value is an integer and the new value is a whole float, keep integer.
        ConfigValue toStore = value;
        ConfigValue existing = Get(dottedKey);
        if (existing.type == ConfigValue::Type::Int &&
            toStore.type == ConfigValue::Type::Float &&
            toStore.f == (double)(int64_t)toStore.f)
        {
            toStore = ConfigValue((int64_t)toStore.f);
        }

        WriteNode(m_Table, dottedKey, toStore);      // merged view (what Get reads)
        WriteNode(m_UserTable, dottedKey, toStore);  // user delta (what SaveToFile writes)

        if (auto it = m_Handlers.find(dottedKey); it != m_Handlers.end())
            for (auto& h : it->second) h(toStore);
    }

    void Subscribe(const std::string& dottedKey, Handler handler)
    {
        // Fire immediately when a value already exists so the module picks up
        // the persisted setting at startup.
        ConfigValue cur = Get(dottedKey);
        if (!cur.IsNone()) handler(cur);
        m_Handlers[dottedKey].push_back(std::move(handler));
    }

    //-------------------------------------------------------------------------
    // Atomic save of the USER layer only: write to "<path>.tmp", then rename
    // over the original. A crash mid-write leaves the previous file intact.
    // config.toml (defaults + comments) is never written by the game.
    //-------------------------------------------------------------------------
    bool SaveToFile(const std::string& filePath = "")
    {
        const std::string path = filePath.empty() ? m_UserFilePath : filePath;
        if (path.empty()) return false;

        const std::string tmp = path + ".tmp";
        {
            std::ofstream ofs(tmp, std::ios::trunc);
            if (!ofs) return false;
            ofs << "# Auto-generated by TriggerOn — user overrides for config.toml.\n"
                   "# Do not hand-edit while the game is running; it rewrites this file.\n\n";
            ofs << m_UserTable;
            if (!ofs) return false;
        }
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);   // atomic on same volume (MoveFileEx)
        if (ec) {
            std::filesystem::remove(tmp, ec);
            return false;
        }
        return true;
    }

    //-------------------------------------------------------------------------
    // Generic typed getters:  Config::GetInstance().GetString("network", "server_host", "127.0.0.1")
    //-------------------------------------------------------------------------
    std::string GetString(const std::string& section, const std::string& key, const std::string& defaultValue = "") const
    {
        if (auto val = m_Table[section][key].value<std::string>())
            return *val;
        return defaultValue;
    }

    int GetInt(const std::string& section, const std::string& key, int defaultValue = 0) const
    {
        if (auto val = m_Table[section][key].value<int64_t>())
            return static_cast<int>(*val);
        return defaultValue;
    }

    double GetDouble(const std::string& section, const std::string& key, double defaultValue = 0.0) const
    {
        if (auto val = m_Table[section][key].value<double>())
            return *val;
        return defaultValue;
    }

    bool GetBool(const std::string& section, const std::string& key, bool defaultValue = false) const
    {
        if (auto val = m_Table[section][key].value<bool>())
            return *val;
        return defaultValue;
    }

    //-------------------------------------------------------------------------
    // Convenience accessors for common settings
    //-------------------------------------------------------------------------
    std::string NetworkMode() const { return GetString("network", "mode",        "mock"); }
    std::string LocalHost()   const { return GetString("network", "local_host",  "127.0.0.1"); }
    std::string RemoteHost()  const { return GetString("network", "remote_host", "127.0.0.1"); }
    int         ServerPort()  const { return GetInt   ("network", "server_port", 7777); }
    double      TickRate()    const { return GetDouble("server",  "tick_rate",   32.0); }

private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // Split "a.b.c" on '.' — our schema is "section.key" but arbitrary depth works
    static std::vector<std::string> SplitKey(const std::string& dotted)
    {
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t i = 0; i <= dotted.size(); ++i) {
            if (i == dotted.size() || dotted[i] == '.') {
                if (i > start) parts.emplace_back(dotted.substr(start, i - start));
                start = i + 1;
            }
        }
        return parts;
    }

    const toml::node* WalkToNode(const std::string& dottedKey) const
    {
        auto parts = SplitKey(dottedKey);
        if (parts.empty()) return nullptr;
        const toml::table* tbl = &m_Table;
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            const toml::node* n = tbl->get(parts[i]);
            if (!n || !n->is_table()) return nullptr;
            tbl = n->as_table();
        }
        return tbl->get(parts.back());
    }

    static void WriteNode(toml::table& root, const std::string& dottedKey, const ConfigValue& v)
    {
        auto parts = SplitKey(dottedKey);
        if (parts.empty()) return;
        toml::table* tbl = &root;
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            toml::node* n = tbl->get(parts[i]);
            if (!n || !n->is_table()) {
                tbl->insert_or_assign(parts[i], toml::table{});
                n = tbl->get(parts[i]);
            }
            tbl = n->as_table();
        }
        const std::string& key = parts.back();
        switch (v.type) {
        case ConfigValue::Type::Bool:   tbl->insert_or_assign(key, v.b); break;
        case ConfigValue::Type::Int:    tbl->insert_or_assign(key, v.i); break;
        case ConfigValue::Type::Float:  tbl->insert_or_assign(key, v.f); break;
        case ConfigValue::Type::String: tbl->insert_or_assign(key, v.s); break;
        case ConfigValue::Type::None:   break;
        }
    }

    // Deep-merge: every leaf in `overlay` is copied over `base` (tables recurse)
    static void MergeInto(toml::table& base, const toml::table& overlay)
    {
        for (auto&& [key, node] : overlay) {
            std::string k(key.str());
            if (node.is_table()) {
                toml::node* existing = base.get(k);
                if (!existing || !existing->is_table()) {
                    base.insert_or_assign(k, toml::table{});
                    existing = base.get(k);
                }
                MergeInto(*existing->as_table(), *node.as_table());
            }
            else if (node.is_boolean())        base.insert_or_assign(k, node.as_boolean()->get());
            else if (node.is_integer())        base.insert_or_assign(k, node.as_integer()->get());
            else if (node.is_floating_point()) base.insert_or_assign(k, node.as_floating_point()->get());
            else if (node.is_string())         base.insert_or_assign(k, node.as_string()->get());
            // arrays / dates not used by our schema — skipped
        }
    }

    toml::table m_Table;       // merged view: defaults + user overrides (Get reads this)
    toml::table m_UserTable;   // user overrides only (SaveToFile writes this)
    std::string m_FilePath;
    std::string m_UserFilePath;
    std::unordered_map<std::string, std::vector<Handler>> m_Handlers;
    bool m_Loaded = false;
};
