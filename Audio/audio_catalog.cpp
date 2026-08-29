//=============================================================================
// audio_catalog.cpp
//=============================================================================
#include "audio_catalog.h"

#define TOML_HEADER_ONLY 1
#include "../ThirdParty/toml++/toml.hpp"

#include <cstdio>
#include <filesystem>

namespace {

// Index-aligned with SoundId.  A mismatch here is a compile-time error thanks
// to the static_assert below, which is the whole point of keeping it adjacent.
const char* const kSoundKeys[] = {
    "weapon_fire",
    "weapon_fire_empty",
    "weapon_reload",
    "weapon_reload_empty",
    "footstep",
    "jump_start",
    "jump_land",
    "hitmarker",
    "take_damage",
    "death",
    "kill_confirm",
    "ads_in",
    "ads_out",
    "ui_click",
    "ambient_loop",
};
static_assert(sizeof(kSoundKeys) / sizeof(kSoundKeys[0]) == (size_t)SoundId::Count,
              "kSoundKeys must stay index-aligned with SoundId");

SoundDef g_Defs[(size_t)SoundId::Count];
const SoundDef kInvalid{};

AudioBus ParseBus(std::string_view s)
{
    if (s == "ui")      return AudioBus::Ui;
    if (s == "music")   return AudioBus::Music;
    if (s == "ambient") return AudioBus::Ambient;
    if (s == "master")  return AudioBus::Master;
    return AudioBus::Sfx;
}

} // namespace

bool AudioCatalog_Load(const char* tomlPath)
{
    // Parse into a local table first.  Nothing below may touch g_Defs until
    // the parse itself has succeeded: a malformed reload must return false
    // without disturbing whatever catalog was already loaded and working.
    toml::table tbl;
    try {
        tbl = toml::parse_file(tomlPath);
    } catch (const toml::parse_error& e) {
        std::printf("[audio] catalog parse failed (%s): %s\n", tomlPath, e.description().data());
        return false;
    }

    // Likewise stage the per-entry results locally and only commit them to
    // g_Defs once the whole pass is done, so a reload is all-or-nothing with
    // respect to the previous good state (individual entries still degrade
    // independently within the new load, same as before).
    SoundDef staged[(size_t)SoundId::Count];

    int loaded = 0;
    for (size_t i = 0; i < (size_t)SoundId::Count; ++i) {
        const auto* node = tbl[kSoundKeys[i]].as_table();
        if (!node) {
            std::printf("[audio] catalog: no table for '%s'\n", kSoundKeys[i]);
            continue;
        }

        SoundDef d{};
        if (const auto* arr = (*node)["files"].as_array()) {
            for (const auto& elem : *arr) {
                if (auto s = elem.value<std::string>()) {
                    // Drop entries whose file is absent rather than failing the
                    // whole catalog: one missing wav silences one variant.
                    if (std::filesystem::exists(*s)) d.files.push_back(*s);
                    else std::printf("[audio] catalog: '%s' file missing: %s\n", kSoundKeys[i], s->c_str());
                }
            }
        }

        d.bus          = ParseBus((*node)["bus"].value_or<std::string>("sfx"));
        d.loop         = (*node)["loop"].value_or(false);
        d.gainDb       = (*node)["gain_db"].value_or(0.0f);
        d.pitchCents   = (*node)["pitch_cents"].value_or(0.0f);
        d.maxInstances = (uint8_t)(*node)["max_instances"].value_or(4);
        d.priority     = (uint8_t)(*node)["priority"].value_or(128);
        d.minDistance  = (*node)["min_distance"].value_or(1.0f);
        d.maxDistance  = (*node)["max_distance"].value_or(60.0f);
        d.rolloff      = (*node)["rolloff"].value_or(1.0f);

        if (d.maxInstances == 0) d.maxInstances = 1;

        staged[i] = std::move(d);
        if (staged[i].IsValid()) ++loaded;
    }

    for (size_t i = 0; i < (size_t)SoundId::Count; ++i) g_Defs[i] = std::move(staged[i]);

    std::printf("[audio] catalog: %d/%d entries loaded from %s\n",
                loaded, (int)SoundId::Count, tomlPath);
    return loaded > 0;
}

const SoundDef& AudioCatalog_Get(SoundId id)
{
    const size_t i = (size_t)id;
    return (i < (size_t)SoundId::Count) ? g_Defs[i] : kInvalid;
}

const char* AudioCatalog_KeyOf(SoundId id)
{
    const size_t i = (size_t)id;
    return (i < (size_t)SoundId::Count) ? kSoundKeys[i] : "<invalid>";
}
