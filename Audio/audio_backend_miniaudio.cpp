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
//
// THREADING CONTRACT
//
// Every piece of state this file owns — g_Pools and the Voice objects inside
// them, g_GroupInited, g_EngineInited, g_PlayCounter, g_Rng, g_Preloaded — is
// touched from the game thread ONLY.  There is no lock and none is needed:
// Audio_* is called from the frame loop, and this file installs no miniaudio
// callback of its own, so no code here ever runs on the device thread.
//
// Everything that does cross to the device thread crosses inside miniaudio: a
// started ma_sound is pulled by the device callback through the node graph,
// and the setters used below (volume, pitch, position, looping, start/stop)
// publish through miniaudio's own atomics, which is why calling them from the
// game thread while the sound is audible is safe rather than a data race.
//
// The one non-obvious consequence is that releasing a voice mid-playback is
// safe.  ma_sound_uninit detaches the node from the graph and blocks until any
// in-flight processing of that node has finished before it frees anything, so
// ReleaseVoice() cannot pull memory out from under a callback that is reading
// it — which is what makes voice stealing (AcquireVoice/CheapestActive) a
// legitimate strategy instead of a race with the mixer.
//=============================================================================
#include "audio_backend.h"
#include "audio_catalog.h"
#include "debug_log.h"

#include "miniaudio.h"

#include <cmath>
#include <random>
#include <vector>

namespace {

// Handle layout: [31:24] SoundId | [23:16] index | [15:0] generation.
// SoundId::Count is 14 and maxInstances is a uint8_t, so both fit with room
// to spare; encoding the pool directly means resolution is a plain decode
// (SoundId -> pool -> slot -> generation check) with no side table needed.
constexpr uint32_t kGenBits    = 16;
constexpr uint32_t kIndexBits  = 8;
constexpr uint32_t kGenMask    = (1u << kGenBits) - 1;
constexpr uint32_t kIndexMask  = (1u << kIndexBits) - 1;
constexpr uint32_t kIndexShift = kGenBits;
constexpr uint32_t kIdShift    = kGenBits + kIndexBits;

struct Voice {
    // RELOCATION INVARIANT: ma_sound is stored BY VALUE and, once started, the
    // engine's node graph holds a raw pointer to this very object.  A vector
    // reallocation while any voice is live would leave the device thread
    // dereferencing freed memory.  This is safe only because Pool::voices is
    // sized exactly once, in Initialize(), from an empty vector, and is never
    // resized, push_back'd or erased afterwards (Finalize() clears it only
    // after every voice has been uninited).  Anything that would grow a pool
    // at runtime must switch to stable storage (std::deque, or heap-allocated
    // ma_sound) first — this is a precondition, not a preference.
    ma_sound sound{};
    bool     inited     = false;
    bool     looping    = false;
    // Starts at 1 so handle bits are never 0; wraps within 16 bits to match
    // the generation field's width in the handle (see ReleaseVoice).
    uint32_t generation = 1;
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

// Every catalog file registered with the engine's resource manager at init, so
// Finalize() can unregister exactly what it registered.  See PreloadCatalog().
std::vector<std::string> g_Preloaded;

ma_sound_group* GroupFor(AudioBus bus)
{
    const size_t i = (size_t)bus;
    if (bus == AudioBus::Master || i >= (size_t)AudioBus::Count) return nullptr;
    return g_GroupInited[i] ? &g_Groups[i] : nullptr;
}

float DbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }

// cents -> playback rate multiplier.  1200 cents = one octave = 2x.
float CentsToRatio(float cents) { return std::pow(2.0f, cents / 1200.0f); }

uint32_t MakeHandle(SoundId id, uint32_t index, uint32_t generation)
{
    return ((uint32_t)id << kIdShift) | ((index & kIndexMask) << kIndexShift) | (generation & kGenMask);
}

// Direct decode of the layout above — no side table, so a stale or corrupt
// handle can only ever miss (nullptr), never hit the wrong pool's voice.
Voice* ResolveLoop(uint32_t handleBits)
{
    const uint32_t idValue = handleBits >> kIdShift;
    const uint32_t index   = (handleBits >> kIndexShift) & kIndexMask;
    const uint32_t gen     = handleBits & kGenMask;

    if (idValue >= (uint32_t)SoundId::Count) return nullptr;
    Pool& pool = g_Pools[idValue];
    if (index >= pool.voices.size()) return nullptr;

    Voice& v = pool.voices[index];
    return (v.inited && v.generation == gen) ? &v : nullptr;
}

void ReleaseVoice(Voice& v)
{
    if (!v.inited) return;
    ma_sound_stop(&v.sound);
    ma_sound_uninit(&v.sound);
    v.inited  = false;
    v.looping = false;
    // Wrap within the handle's 16-bit generation field, not the full 32 bits
    // of the counter itself, so ResolveLoop's masked comparison stays valid
    // forever instead of going permanently stale after 65536 releases.
    v.generation = (v.generation + 1) & kGenMask;
    if (v.generation == 0) v.generation = 1;   // 0 is reserved for "no handle"
}

// Global voice budget.  Per-sound pools cap how many of ONE sound can overlap;
// this caps the total, which is what actually protects the mix when several
// sounds are all near their own limit at once.  It is also the only place
// SoundDef::priority can matter — within a pool every voice is the same sound
// and therefore the same priority, so stealing there is purely oldest-first.
constexpr int kMaxTotalVoices = 48;

// Also what AudioBackend::ActiveVoiceCount() reports to the debug overlay: one
// loop, so the budget the allocator enforces and the number on screen cannot
// drift apart.
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
    // Advance the cursor here too, exactly as the free-slot path does: the
    // round-robin scan should resume past the slot we just handed out whichever
    // branch produced it.
    pool.next = (oldest + 1) % pool.voices.size();
    outIndex  = oldest;
    return &pool.voices[oldest];
}

bool StartVoice(Voice& v, const SoundDef& def, SoundId id,
                const DirectX::XMFLOAT3* world, float gainScale, bool loop)
{
    ReleaseVoice(v);

    // Both callers check IsValid() first, but the distribution below would be
    // (0, SIZE_MAX) on an empty files[] and index far out of bounds, so the
    // guard lives here where the hazard is rather than in the callers where a
    // future refactor could quietly drop it.
    if (def.files.empty()) return false;

    const std::string& file =
        def.files[std::uniform_int_distribution<size_t>(0, def.files.size() - 1)(g_Rng)];

    // Streamed entries page off disk instead of holding decoded PCM; everything
    // else is pre-decoded and pinned by Initialize()'s pre-pass, so DECODE here
    // resolves against a warm resource-manager buffer with no file I/O on the
    // game thread.  LOOPING has to be set at init for a streaming loop: the
    // stream pre-fills its first page during init and would leave a gap at the
    // wrap point if it did not already know it was going to loop.
    ma_uint32 flags = def.stream ? (ma_uint32)MA_SOUND_FLAG_STREAM
                                 : (ma_uint32)MA_SOUND_FLAG_DECODE;
    if (def.stream && loop) flags |= (ma_uint32)MA_SOUND_FLAG_LOOPING;
    if (!world) flags |= (ma_uint32)MA_SOUND_FLAG_NO_SPATIALIZATION;

    if (ma_sound_init_from_file(&g_Engine, file.c_str(), flags,
                                GroupFor(def.bus), nullptr, &v.sound) != MA_SUCCESS) {
        DebugLog_Printf("audio", "failed to load %s", file.c_str());
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

// Pull every non-streaming catalog file into the resource manager once, at
// init, and hold a reference for the process lifetime.
//
// Without this, every cold ma_sound_init_from_file below opens and decodes its
// WAV synchronously on the game thread.  The resource manager does cache
// decoded buffers by path, but it drops them at refcount zero, and
// ReclaimVoices() releases every finished one-shot each frame — so the cache is
// empty for any sound that is not currently audible.  The first shot of a
// burst, and every footstep variant that happens not to be playing, paid a full
// open + decode inside the frame.  The realistic worst case is several 250-360
// KB fire variants decoded in one frame when a firefight resumes.
//
// Registering here makes the refcount never reach zero, so later inits resolve
// against a warm buffer and do no I/O at all.  The whole catalog is ~2 MB of
// short one-shots; the one genuinely large asset, the ambient bed, is marked
// `stream` and deliberately skipped (it never wants a resident decode).
//
// A file that fails to register is simply left out: it will fall back to the
// old cold-load path, which still works, just slowly.  Audio never fails hard.
void PreloadCatalog()
{
    ma_resource_manager* rm = ma_engine_get_resource_manager(&g_Engine);
    if (!rm) {
        DebugLog_Printf("audio", "no resource manager - sounds will load on first play");
        return;
    }

    int ok = 0;
    for (size_t i = 0; i < (size_t)SoundId::Count; ++i) {
        const SoundDef& def = AudioCatalog_Get((SoundId)i);
        if (def.stream) continue;
        for (const std::string& file : def.files) {
            const ma_result r = ma_resource_manager_register_file(
                rm, file.c_str(), (ma_uint32)MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE);
            if (r != MA_SUCCESS) {
                DebugLog_Printf("audio", "preload failed (%s) - will load on demand", file.c_str());
                continue;
            }
            g_Preloaded.push_back(file);
            ++ok;
        }
    }
    DebugLog_Printf("audio", "preloaded %d catalog files", ok);
}

void UnloadCatalog()
{
    ma_resource_manager* rm = ma_engine_get_resource_manager(&g_Engine);
    if (rm) {
        for (const std::string& file : g_Preloaded)
            ma_resource_manager_unregister_file(rm, file.c_str());
    }
    g_Preloaded.clear();
}

} // namespace

namespace AudioBackend {

bool Initialize()
{
    if (ma_engine_init(nullptr, &g_Engine) != MA_SUCCESS) {
        DebugLog_Printf("audio", "ma_engine_init failed");
        return false;
    }
    g_EngineInited = true;

    for (size_t i = 0; i < (size_t)AudioBus::Count; ++i) {
        if ((AudioBus)i == AudioBus::Master) continue;
        if (ma_sound_group_init(&g_Engine, 0, nullptr, &g_Groups[i]) != MA_SUCCESS) {
            DebugLog_Printf("audio", "sound group init failed");
            Finalize();
            return false;
        }
        g_GroupInited[i] = true;
    }

    for (size_t i = 0; i < (size_t)SoundId::Count; ++i) {
        const SoundDef& def = AudioCatalog_Get((SoundId)i);
        g_Pools[i].voices.resize(def.IsValid() ? def.maxInstances : 0);
    }

    PreloadCatalog();

    return true;
}

void Finalize()
{
    for (Pool& pool : g_Pools) {
        for (Voice& v : pool.voices) ReleaseVoice(v);
        pool.voices.clear();
    }

    // Drop the init-time references before the engine (and with it the resource
    // manager that owns those buffers) goes away.  Voices first: a registered
    // buffer must outlive every sound reading it.
    UnloadCatalog();

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

void ReclaimVoices()
{
    // Reclaim finished one-shots so their slots and their share of the global
    // budget are free before this frame's plays ask for either.  Loops are
    // never reclaimed here: they end via StopLoop, or when their own pool
    // saturates and AcquireVoice reuses the oldest slot in it.  They are NOT
    // stolen for the global budget — CheapestActive skips looping voices on
    // purpose, so the ambient bed cannot be culled by a firefight.
    for (Pool& pool : g_Pools) {
        for (Voice& v : pool.voices) {
            if (v.inited && !v.looping && !ma_sound_is_playing(&v.sound)) ReleaseVoice(v);
        }
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
    if (!v) return 0;
    if (!StartVoice(*v, def, id, world, 1.0f, true)) {
        // StartVoice can fail after ma_sound_init_from_file already succeeded
        // (i.e. ma_sound_start itself failed), leaving the voice inited and
        // marked looping. ReclaimVoices() only reclaims !looping voices, so
        // without this the slot would leak forever, counting against the
        // global voice budget with no handle able to reach it. Safe to call
        // even when init failed: ReleaseVoice no-ops on an uninited voice.
        ReleaseVoice(*v);
        return 0;
    }

    return MakeHandle(id, (uint32_t)index, v->generation);
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
    return TotalActiveVoices();
}

} // namespace AudioBackend
