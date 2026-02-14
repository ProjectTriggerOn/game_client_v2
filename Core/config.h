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

// toml++ requires this define for header-only mode
#define TOML_HEADER_ONLY 1
#include "../ThirdParty/toml++/toml.hpp"

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
    void Load(const std::string& filePath)
    {
        m_Table = toml::parse_file(filePath);
        m_Loaded = true;
    }

    bool IsLoaded() const { return m_Loaded; }

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

    toml::table m_Table;
    bool m_Loaded = false;
};
