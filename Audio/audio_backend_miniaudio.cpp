//=============================================================================
// audio_backend_miniaudio.cpp
//
// The only TU besides miniaudio_impl.cpp that sees miniaudio.  Owns the engine,
// the four sound groups and the per-sound voice pools.
//
// Why a hand-rolled pool instead of miniaudio's fire-and-forget playback: the
// inline API frees voices for you but hands back no handle, so there is no way
// to cap concurrency.  With 60Hz snapshots and packet-loss catch-up, a dozen
// gunshots can land in one frame; without a cap they all play and the mix
// clips.  The pool caps per sound and steals by priority.
//=============================================================================
#include "audio_backend.h"
#include "audio_catalog.h"

#include "miniaudio.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace {

constexpr uint32_t kIndexBits = 16;
constexpr uint32_t kIndexMask = (1u << kIndexBits) - 1;

struct Voice {
    ma_sound sound{};
    bool     inited     = false;
    bool     looping    = false;
    uint32_t generation = 1;      // starts at 1 so handle bits are never 0
    uint64_t startOrder = 0;      // for oldest-first stealing
    SoundId  id         = SoundId::Count;
};

struct Pool {
    std::vector<Voice> voices;    // sized from SoundDef::maxInstances
    size_t             next = 0;  // round-robin cursor
};

ma_engine      g_Engine{};
ma_sound_group g_Groups[(size_t)AudioBus::Count]{};   // Master slot unused: that is the engine
bool           g_GroupInited[(size_t)AudioBus::Count]{};
Pool           g_Pools[(size_t)SoundId::Count];
bool           g_EngineInited = false;
uint64_t       g_PlayCounter  = 0;

std::mt19937 g_Rng{ 0xA0D10 };

ma_sound_group* GroupFor(AudioBus bus)
{
    const size_t i = (size_t)bus;
    if (bus == AudioBus::Master || i >= (size_t)AudioBus::Count) return nullptr;
    return g_GroupInited[i] ? &g_Groups[i] : nullptr;
}

float DbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }

// cents -> playback rate multiplier.  1200 cents = one octave = 2x.
float CentsToRatio(float cents) { return std::pow(2.0f, cents / 1200.0f); }

uint32_t MakeHandle(uint32_t index, uint32_t generation)
{
    return ((generation & 0xFFFFu) << kIndexBits) | (index & kIndexMask);
}

// A handle also has to carry which pool it came from.  Rather than widen the
// handle we keep a flat side table of every looping voice; loops are few
// (ambience plus at most one footstep loop per player) so a linear scan is
// cheaper than the bookkeeping alternative.
struct LoopRef { SoundId id; size_t index; uint32_t generation; };
std::vector<LoopRef> g_Loops;

Voice* ResolveLoop(uint32_t handleBits)
{
    const uint32_t index = handleBits & kIndexMask;
    const uint32_t gen   = (handleBits >> kIndexBits) & 0xFFFFu;
    for (const LoopRef& ref : g_Loops) {
        if (ref.index != index || ref.generation != gen) continue;
        Pool& pool = g_Pools[(size_t)ref.id];
        if (index >= pool.voices.size()) return nullptr;
        Voice& v = pool.voices[index];
        return (v.generation == gen && v.inited) ? &v : nullptr;
    }
    return nullptr;
}

void ReleaseVoice(Voice& v)
{
    if (!v.inited) return;
    ma_sound_stop(&v.sound);
    ma_sound_uninit(&v.sound);
    v.inited  = false;
    v.looping = false;
    if (++v.generation == 0) v.generation = 1;   // 0 is reserved for "no handle"
}

// Global voice budget.  Per-sound pools cap how many of ONE sound can overlap;
// this caps the total, which is what actually protects the mix when several
// sounds are all near their own limit at once.  It is also the only place
// SoundDef::priority can matter — within a pool every voice is the same sound
// and therefore the same priority, so stealing there is purely oldest-first.
constexpr int kMaxTotalVoices = 48;

int TotalActiveVoices()
{
    int n = 0;
    for (const Pool& p : g_Pools)
        for (const Voice& v : p.voices)
            if (v.inited) ++n;
    return n;
}

// The cheapest voice playing anywhere, so a gunshot can take a footstep's slot
// once the global budget is spent.  Loops are excluded: the ambient bed getting
// culled by a firefight would be a worse artefact than one dropped footstep.
Voice* CheapestActive(uint8_t& outPriority)
{
    Voice*  worst     = nullptr;
    uint8_t worstPrio = 0xFF;
    uint64_t worstAge = UINT64_MAX;

    for (Pool& p : g_Pools) {
        for (Voice& v : p.voices) {
            if (!v.inited || v.looping) continue;
            const uint8_t prio = AudioCatalog_Get(v.id).priority;
            if (prio < worstPrio || (prio == worstPrio && v.startOrder < worstAge)) {
                worstPrio = prio;
                worstAge  = v.startOrder;
                worst     = &v;
            }
        }
    }
    outPriority = worstPrio;
    return worst;
}

// Pick a slot.  Returns nullptr when this sound cannot be afforded, which is
// the correct outcome — dropping a footstep to keep a gunshot.
Voice* AcquireVoice(Pool& pool, const SoundDef& def, size_t& outIndex)
{
    for (size_t n = 0; n < pool.voices.size(); ++n) {
        const size_t i = (pool.next + n) % pool.voices.size();
        Voice& v = pool.voices[i];
        if (!v.inited || !ma_sound_is_playing(&v.sound)) {
            // A free slot in our own pool still costs global budget.
            if (v.inited == false && TotalActiveVoices() >= kMaxTotalVoices) {
                uint8_t cheapestPrio = 0xFF;
                Voice*  cheapest     = CheapestActive(cheapestPrio);
                if (!cheapest || cheapestPrio >= def.priority) return nullptr;
                ReleaseVoice(*cheapest);
            }
            pool.next = (i + 1) % pool.voices.size();
            outIndex  = i;
            return &v;
        }
    }

    // Pool is saturated: reuse our own oldest.  Total voice count is unchanged,
    // so the global budget does not come into it.
    size_t   oldest      = 0;
    uint64_t oldestOrder = UINT64_MAX;
    for (size_t i = 0; i < pool.voices.size(); ++i) {
        if (pool.voices[i].startOrder < oldestOrder) {
            oldestOrder = pool.voices[i].startOrder;
            oldest      = i;
        }
    }
    outIndex = oldest;
    return &pool.voices[oldest];
}

bool StartVoice(Voice& v, const SoundDef& def, SoundId id,
                const DirectX::XMFLOAT3* world, float gainScale, bool loop)
{
    ReleaseVoice(v);

    const std::string& file =
        def.files[std::uniform_int_distribution<size_t>(0, def.files.size() - 1)(g_Rng)];

    ma_uint32 flags = (ma_uint32)MA_SOUND_FLAG_DECODE;
    if (!world) flags |= (ma_uint32)MA_SOUND_FLAG_NO_SPATIALIZATION;

    if (ma_sound_init_from_file(&g_Engine, file.c_str(), flags,
                                GroupFor(def.bus), nullptr, &v.sound) != MA_SUCCESS) {
        std::printf("[audio] failed to load %s\n", file.c_str());
        return false;
    }
    v.inited = true;
    v.id     = id;

    ma_sound_set_volume(&v.sound, DbToLinear(def.gainDb) * gainScale);

    if (def.pitchCents > 0.0f) {
        std::uniform_real_distribution<float> d(-def.pitchCents, def.pitchCents);
        ma_sound_set_pitch(&v.sound, CentsToRatio(d(g_Rng)));
    }

    if (world) {
        ma_sound_set_position(&v.sound, world->x, world->y, world->z);
        ma_sound_set_attenuation_model(&v.sound, ma_attenuation_model_inverse);
        ma_sound_set_min_distance(&v.sound, def.minDistance);
        ma_sound_set_max_distance(&v.sound, def.maxDistance);
        ma_sound_set_rolloff(&v.sound, def.rolloff);
        // Doppler off: FPS convention.  With it on, a strafing shooter's gun
        // audibly detunes, which reads as a bug rather than as motion.
        ma_sound_set_doppler_factor(&v.sound, 0.0f);
    }

    ma_sound_set_looping(&v.sound, loop ? MA_TRUE : MA_FALSE);
    v.looping    = loop;
    v.startOrder = ++g_PlayCounter;

    return ma_sound_start(&v.sound) == MA_SUCCESS;
}

} // namespace

namespace AudioBackend {

bool Initialize()
{
    if (ma_engine_init(nullptr, &g_Engine) != MA_SUCCESS) {
        std::printf("[audio] ma_engine_init failed\n");
        return false;
    }
    g_EngineInited = true;

    for (size_t i = 0; i < (size_t)AudioBus::Count; ++i) {
        if ((AudioBus)i == AudioBus::Master) continue;
        if (ma_sound_group_init(&g_Engine, 0, nullptr, &g_Groups[i]) != MA_SUCCESS) {
            std::printf("[audio] sound group init failed\n");
            Finalize();
            return false;
        }
        g_GroupInited[i] = true;
    }

    for (size_t i = 0; i < (size_t)SoundId::Count; ++i) {
        const SoundDef& def = AudioCatalog_Get((SoundId)i);
        g_Pools[i].voices.resize(def.IsValid() ? def.maxInstances : 0);
    }

    return true;
}

void Finalize()
{
    for (Pool& pool : g_Pools) {
        for (Voice& v : pool.voices) ReleaseVoice(v);
        pool.voices.clear();
    }
    g_Loops.clear();

    for (size_t i = 0; i < (size_t)AudioBus::Count; ++i) {
        if (!g_GroupInited[i]) continue;
        ma_sound_group_uninit(&g_Groups[i]);
        g_GroupInited[i] = false;
    }

    if (g_EngineInited) {
        ma_engine_uninit(&g_Engine);
        g_EngineInited = false;
    }
}

void SetListener(const AudioListener& l)
{
    if (!g_EngineInited) return;
    ma_engine_listener_set_position (&g_Engine, 0, l.position.x, l.position.y, l.position.z);
    ma_engine_listener_set_direction(&g_Engine, 0, l.front.x,    l.front.y,    l.front.z);
    ma_engine_listener_set_world_up (&g_Engine, 0, l.up.x,       l.up.y,       l.up.z);
}

void Update(double)
{
    // Reclaim finished one-shots so their decoded buffers do not pile up, and
    // retire the loop refs whose voice has been stolen out from under them.
    for (Pool& pool : g_Pools) {
        for (Voice& v : pool.voices) {
            if (v.inited && !v.looping && !ma_sound_is_playing(&v.sound)) ReleaseVoice(v);
        }
    }
    for (size_t i = 0; i < g_Loops.size();) {
        Pool& pool = g_Pools[(size_t)g_Loops[i].id];
        const size_t idx = g_Loops[i].index;
        const bool alive = idx < pool.voices.size()
                        && pool.voices[idx].inited
                        && pool.voices[idx].generation == g_Loops[i].generation;
        if (alive) ++i;
        else g_Loops.erase(g_Loops.begin() + (long)i);
    }
}

void PlayOneShot(SoundId id, const DirectX::XMFLOAT3* world, float gainScale)
{
    if (!g_EngineInited || (size_t)id >= (size_t)SoundId::Count) return;
    const SoundDef& def = AudioCatalog_Get(id);
    if (!def.IsValid()) return;

    Pool& pool = g_Pools[(size_t)id];
    if (pool.voices.empty()) return;

    size_t index = 0;
    Voice* v = AcquireVoice(pool, def, index);
    if (v) StartVoice(*v, def, id, world, gainScale, false);
}

uint32_t PlayLoop(SoundId id, const DirectX::XMFLOAT3* world)
{
    if (!g_EngineInited || (size_t)id >= (size_t)SoundId::Count) return 0;
    const SoundDef& def = AudioCatalog_Get(id);
    if (!def.IsValid()) return 0;

    Pool& pool = g_Pools[(size_t)id];
    if (pool.voices.empty()) return 0;

    size_t index = 0;
    Voice* v = AcquireVoice(pool, def, index);
    if (!v || !StartVoice(*v, def, id, world, 1.0f, true)) return 0;

    g_Loops.push_back(LoopRef{ id, index, v->generation });
    return MakeHandle((uint32_t)index, v->generation);
}

void StopLoop(uint32_t handleBits)
{
    if (Voice* v = ResolveLoop(handleBits)) ReleaseVoice(*v);
}

void SetLoopPosition(uint32_t handleBits, const DirectX::XMFLOAT3& world)
{
    if (Voice* v = ResolveLoop(handleBits))
        ma_sound_set_position(&v->sound, world.x, world.y, world.z);
}

void SetBusVolume(AudioBus bus, float linear01)
{
    if (!g_EngineInited) return;
    if (linear01 < 0.0f) linear01 = 0.0f;
    if (linear01 > 1.0f) linear01 = 1.0f;

    if (bus == AudioBus::Master) { ma_engine_set_volume(&g_Engine, linear01); return; }
    if (ma_sound_group* g = GroupFor(bus)) ma_sound_group_set_volume(g, linear01);
}

int ActiveVoiceCount()
{
    int n = 0;
    for (const Pool& pool : g_Pools)
        for (const Voice& v : pool.voices)
            if (v.inited) ++n;
    return n;
}

} // namespace AudioBackend
