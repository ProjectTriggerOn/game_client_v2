#pragma once
//=============================================================================
// mock_server.h
//
// Mock Server with fixed-tick (32Hz) game logic.
// Uses accumulator pattern to decouple from render frame rate.
//
// Architecture:
//   - Server runs at fixed 32Hz tick rate
//   - Consumes InputCmd from client
//   - Produces authoritative PlayerState
//   - Broadcasts Snapshot to client
//=============================================================================

#include "net_common.h"
#include "collision_world.h"

class INetwork;

class MockServer
{
public:
    //-------------------------------------------------------------------------
    // Constants
    //-------------------------------------------------------------------------
    static constexpr double TICK_RATE = 32.0;                    // 32 ticks per second
    static constexpr double TICK_DURATION = 1.0 / TICK_RATE;     // ~31.25ms per tick

    MockServer();
    ~MockServer();

    void Initialize(INetwork* pNetwork, CollisionWorld* pCollisionWorld = nullptr);
    void Finalize();

    //-------------------------------------------------------------------------
    // Called every render frame - uses accumulator for fixed tick
    //-------------------------------------------------------------------------
    void Update(double deltaTime);

    //-------------------------------------------------------------------------
    // Getters for debug visualization
    //-------------------------------------------------------------------------
    uint32_t GetCurrentTick() const { return m_CurrentTick; }
    double GetAccumulator() const { return m_Accumulator; }
    double GetServerTime() const { return m_ServerTime; }
    const NetPlayerState& GetPlayerState() const { return m_PlayerState; }

private:
    //-------------------------------------------------------------------------
    // Combat-bot data — every remote bot (ids 1..NUM_BOTS) is a full combat
    // entity. Declared before the private methods so their signatures can take
    // a Bot&.
    //-------------------------------------------------------------------------
    static constexpr int    NUM_BOTS = MAX_PLAYERS - 1;   // 9 remote bots
    static constexpr size_t BOT_HISTORY_SIZE = 64;        // lag-comp ring, 2s @ 32Hz
    static constexpr float  REWIND_TELEPORT_GUARD = 2.0f; // never lerp across a teleport

    struct BotHistoryEntry {
        uint32_t tick = 0;                 // 0 = slot never written
        DirectX::XMFLOAT3 position{};
        bool alive = false;
    };

    struct Bot {
        NetPlayerState    state{};         // wire state sent in the snapshot
        uint8_t           team = PlayerTeam::RED;
        DirectX::XMFLOAT3 base{};          // PaceBot center (spawn point)
        float             phase = 0.0f;    // PaceBot phase offset

        double respawnTimer = 0.0;         // counts down while IS_DEAD

        // Intermittent-fire AI
        uint8_t ammo        = WeaponConfig::MAG_SIZE;
        double  reloadTimer = 0.0;         // >0 while reloading
        double  fireTimer   = 0.0;         // per-shot cadence inside a burst
        double  burstTimer  = 0.0;         // time left in the current burst / gap
        bool    firing      = false;       // currently in a firing burst

        // Lag-compensation position history, written at the end of Tick()
        BotHistoryEntry history[BOT_HISTORY_SIZE]{};
    };

    //-------------------------------------------------------------------------
    // Fixed tick logic (called at exactly 32Hz)
    //-------------------------------------------------------------------------
    void Tick();

    //-------------------------------------------------------------------------
    // Process single input command
    //-------------------------------------------------------------------------
    void ProcessInputCmd(const InputCmd& cmd);

    //-------------------------------------------------------------------------
    // Update reload timer once per tick (BEFORE input processing)
    //-------------------------------------------------------------------------
    void UpdateReloadTimer();

    //-------------------------------------------------------------------------
    // Apply physics simulation for one tick
    //-------------------------------------------------------------------------
    void SimulatePhysics();

    //-------------------------------------------------------------------------
    // Send snapshot to client
    //-------------------------------------------------------------------------
    void BroadcastSnapshot();

    //-------------------------------------------------------------------------
    // Combat: fire-rate gating + hitscan raycast
    //-------------------------------------------------------------------------
    void ProcessFiring();

    //-------------------------------------------------------------------------
    // Advance every remote bot one tick: shared PaceBot movement + intermittent-
    // fire / auto-reload AI + death/respawn.
    //-------------------------------------------------------------------------
    void UpdateBots();

    //-------------------------------------------------------------------------
    // Step one bot's weapon AI: burst firing + auto-reload when the mag empties.
    // Each shot fires a live hitscan (BotFireShot).
    //-------------------------------------------------------------------------
    void UpdateBotWeapon(Bot& bot, int index);

    //-------------------------------------------------------------------------
    // One hitscan shot from a bot along its facing — damages the closest live
    // bot or the local player in front of the nearest wall (bots fire straight
    // ahead; the facing layout produces RED<->BLUE crossfire and bot->player).
    //-------------------------------------------------------------------------
    void BotFireShot(const Bot& shooter, int shooterIndex);

    //-------------------------------------------------------------------------
    // Apply damage (kill + start respawn on lethal) to a bot / to the player.
    //-------------------------------------------------------------------------
    void DamageBot(Bot& bot, uint8_t dmg);
    void DamagePlayer(uint8_t dmg);

    //-------------------------------------------------------------------------
    // Reset a bot to its spawn after the respawn delay.
    //-------------------------------------------------------------------------
    void RespawnBot(Bot& bot, int index);

    //-------------------------------------------------------------------------
    // Count down + apply the local player's respawn (bots can now kill it).
    //-------------------------------------------------------------------------
    void UpdatePlayerRespawn();

    //-------------------------------------------------------------------------
    // Lag compensation: resolve a bot's capsule position at the (fractional)
    // tick the client was viewing. Returns false if the bot was dead then
    // (not hittable — the client rendered no live target there).
    //-------------------------------------------------------------------------
    bool GetRewoundBotPosition(const Bot& bot, uint32_t viewTick, float viewFrac,
                               DirectX::XMFLOAT3& outPos) const;

private:
    INetwork* m_pNetwork;

    // Timing
    double m_Accumulator;           // Time accumulated since last tick
    double m_ServerTime;            // Total server time
    uint32_t m_CurrentTick;         // Current tick number

    // Game State (Server Authoritative)
    NetPlayerState m_PlayerState;
    InputCmd m_LastInputCmd;        // Most recent input from client
    uint32_t m_PrevButtons = 0;     // Previous cmd.buttons for edge detection

    // Reload latch timer — keeps IS_RELOADING active for full animation duration
    double m_ReloadTimer = 0.0;

    // Collision world for gravity
    CollisionWorld* m_pCollisionWorld = nullptr;

    // All remote bots (ids 1..NUM_BOTS). The Bot type is defined above so the
    // private methods can take a Bot&. Movement matches the old layout; each is
    // now a shootable, intermittently-firing, auto-reloading combat bot.
    Bot m_Bots[NUM_BOTS]{};

    // Local player combat state
    double   m_FireTimer = 0.0;
    uint16_t m_FireCounter = 0;
    double   m_PlayerRespawnTimer = 0.0;  // counts down while the player IS_DEAD

    // Ammo
    uint8_t m_Ammo = WeaponConfig::MAG_SIZE;
    uint8_t m_AmmoReserve = WeaponConfig::MAX_RESERVE;

    // Player collision parameters (must match PlayerFps)
    static constexpr float PLAYER_HEIGHT = 1.6f;
    static constexpr float CAPSULE_RADIUS = 0.3f;

    // Weapon parameters (local player = RED team)
    static constexpr uint8_t LOCAL_PLAYER_TEAM = PlayerTeam::RED;  // for friendly-fire checks
    static constexpr double RED_RPM = 600.0;
    static constexpr uint8_t RED_DAMAGE = 34;
    static constexpr uint8_t MAX_HEALTH = 200;
    static constexpr double RESPAWN_TIME = 2.0;

    // Bot intermittent-fire cadence (mock combat bots)
    static constexpr double BOT_FIRE_RPM   = 600.0;   // ammo-burn rate while firing
    static constexpr double BOT_BURST_TIME = 0.5;     // hold fire this long per burst
    static constexpr double BOT_GAP_TIME   = 1.5;     // pause between bursts
    static constexpr float   BOT_EYE_HEIGHT = 1.4f;   // muzzle height for bot hitscan
    static constexpr uint8_t BOT_DAMAGE     = 25;     // damage a bot deals per hit
};
