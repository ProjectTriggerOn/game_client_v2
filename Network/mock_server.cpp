//=============================================================================
// mock_server.cpp
//
// Mock Server implementation with fixed 32Hz tick rate.
// Uses accumulator pattern for frame-rate independent simulation.
//=============================================================================

#include "mock_server.h"
#include "i_network.h"
#include "debug_log.h"
#include <cmath>

MockServer::MockServer()
    : m_pNetwork(nullptr)
    , m_Accumulator(0.0)
    , m_ServerTime(0.0)
    , m_CurrentTick(0)
    , m_PlayerState{}
    , m_LastInputCmd{}
{
}

MockServer::~MockServer()
{
    Finalize();
}

void MockServer::Initialize(INetwork* pNetwork, CollisionWorld* pCollisionWorld)
{
    m_pNetwork = pNetwork;
    m_pCollisionWorld = pCollisionWorld;
    m_Accumulator = 0.0;
    m_ServerTime = 0.0;
    m_CurrentTick = 0;

    // Initialize player state (match PlayerFps spawn in Game_Initialize)
    m_PlayerState.tickId = 0;
    m_PlayerState.position = { -7.0f, 0.0f, -7.0f };  // Corner spawn
    m_PlayerState.velocity = { 0.0f, 0.0f, 0.0f };
    m_PlayerState.yaw = 0.0f;
    m_PlayerState.pitch = 0.0f;
    m_PlayerState.stateFlags = NetStateFlags::IS_GROUNDED;
    m_PlayerState.health = 200;
    m_PlayerState.hitByPlayerId = 0xFF;
    m_PlayerState.fireCounter = 0;
    m_Ammo = WeaponConfig::MAG_SIZE;
    m_AmmoReserve = WeaponConfig::MAX_RESERVE;

    // Initialize last input
    m_LastInputCmd = {};

    m_FireTimer = 0.0;
    m_FireCounter = 0;

    // Initialize all remote bots as combat bots. Bot 0 (id 1) keeps the former
    // combat bot's slot (BLUE, base {7,0,7}, phase 7); bots 1..8 (ids 2..9) keep
    // the former display-bot strip layout (even = RED on -Z, odd = BLUE on +Z).
    // Movement is unchanged — every bot is now shootable, fires intermittently,
    // and auto-reloads (see UpdateBots / UpdateBotWeapon).
    for (int i = 0; i < NUM_BOTS; i++)
    {
        Bot& b = m_Bots[i];
        b = Bot{};

        uint8_t           team;
        DirectX::XMFLOAT3 base;
        float             phase;
        if (i == 0)
        {
            team  = PlayerTeam::BLUE;       // former combat bot
            base  = { 7.0f, 0.0f, 7.0f };
            phase = 7.0f;                   // locked 1m off the adjacent BLUE bot
        }
        else
        {
            int   k     = i - 1;            // 0..7 former display-bot index
            team        = (k % 2 == 0) ? PlayerTeam::RED : PlayerTeam::BLUE;
            int   col   = k / 2;            // 0..3 within each team
            float baseX = -6.0f + col * 4.0f;  // -6, -2, 2, 6
            float baseZ = (team == PlayerTeam::RED) ? -7.0f : 7.0f;
            base  = { baseX, 0.0f, baseZ };
            phase = static_cast<float>(k);
        }

        b.team  = team;
        b.base  = base;
        b.phase = phase;

        NetPlayerState& s = b.state;
        s.position = base;
        // RED (-Z side) faces +Z (yaw 0); BLUE (+Z side) faces -Z (yaw pi).
        s.yaw = (team == PlayerTeam::RED) ? 0.0f : 3.14159265f;
        s.stateFlags = NetStateFlags::IS_GROUNDED;
        s.health = MAX_HEALTH;
        s.hitByPlayerId = 0xFF;
        s.ammo = WeaponConfig::MAG_SIZE;
        s.ammoReserve = WeaponConfig::MAX_RESERVE;

        // Stagger the fire cadence so the bots don't all shoot in unison.
        b.burstTimer = BOT_GAP_TIME + 0.2 * i;
    }
}

void MockServer::Finalize()
{
    m_pNetwork = nullptr;
}

//-----------------------------------------------------------------------------
// Update - Called every render frame
// 
// Accumulator Pattern:
//   1. Add frame delta time to accumulator
//   2. While accumulator >= TICK_DURATION, run one Tick()
//   3. This ensures exactly TICK_RATE ticks per second regardless of FPS
//-----------------------------------------------------------------------------
void MockServer::Update(double deltaTime)
{
    if (!m_pNetwork) return;

    // Clamp deltaTime to prevent spiral of death on frame spikes
    const double maxDelta = TICK_DURATION * 4.0;  // Max 4 ticks per frame
    deltaTime = (deltaTime > maxDelta) ? maxDelta : deltaTime;

    m_Accumulator += deltaTime;

    // ========================================================================
    // ACCUMULATOR LOOP - Core of fixed tick timing
    // This loop runs Tick() at exactly 32Hz regardless of render frame rate
    // ========================================================================
    while (m_Accumulator >= TICK_DURATION)
    {
        Tick();
        m_Accumulator -= TICK_DURATION;
    }
}

//-----------------------------------------------------------------------------
// Tick - Fixed rate game logic (32Hz)
//
// This is where all authoritative game logic runs:
//   1. Update reload timer (before input so freshly-started reloads last full duration)
//   2. Consume input commands from client
//   3. Simulate physics
//   4. Update game state
//   5. Broadcast snapshot to client
//-----------------------------------------------------------------------------
void MockServer::Tick()
{
    m_CurrentTick++;
    m_ServerTime += TICK_DURATION;

    // 0. Local player respawn — bots can now kill the player (see BotFireShot).
    if (m_PlayerState.stateFlags & NetStateFlags::IS_DEAD)
        UpdatePlayerRespawn();
    const bool playerAlive = (m_PlayerState.stateFlags & NetStateFlags::IS_DEAD) == 0;

    // 1. Update reload timer once per tick (BEFORE input so newly-started reloads
    //    this tick last exactly RELOAD_DURATION rather than RELOAD_DURATION - TICK_DURATION)
    if (playerAlive)
        UpdateReloadTimer();

    // 2. Consume all pending input commands (ProcessInputCmd freezes a dead
    //    player's actions to camera-only)
    InputCmd cmd;
    while (m_pNetwork->ReceiveInputCmd(cmd))
    {
        ProcessInputCmd(cmd);
    }

    // 3. Simulate physics for this tick (frozen while dead)
    if (playerAlive)
        SimulatePhysics();

    // 3b. Advance all bots BEFORE firing (mirrors GameServer's physics-before-
    // combat order, so a shot at viewTick == m_CurrentTick tests each bot's
    // post-move live position). Movement + intermittent fire + reload + respawn.
    UpdateBots();

    // 4. Clear the local hit marker, then resolve the local player's shots
    // against every bot. Skip if a bot just killed the player this tick.
    m_PlayerState.hitByPlayerId = 0xFF;
    if ((m_PlayerState.stateFlags & NetStateFlags::IS_DEAD) == 0)
        ProcessFiring();

    // 5. Update tick ID in local state (and ack of last processed input — see GameServer)
    m_PlayerState.tickId = m_CurrentTick;
    m_PlayerState.lastProcessedInputTick = m_LastInputCmd.tickId;
    m_PlayerState.fireCounter = m_FireCounter;
    m_PlayerState.ammo = m_Ammo;
    m_PlayerState.ammoReserve = m_AmmoReserve;

    // 5b. Record lag-compensation history for every bot — exactly the state
    // BroadcastSnapshot() is about to send (incl. respawn teleports).
    for (int i = 0; i < NUM_BOTS; i++)
    {
        Bot& b = m_Bots[i];
        BotHistoryEntry& h = b.history[m_CurrentTick % BOT_HISTORY_SIZE];
        h.tick = m_CurrentTick;
        h.position = b.state.position;
        h.alive = (b.state.stateFlags & NetStateFlags::IS_DEAD) == 0;
    }

    // 6. Broadcast snapshot to client
    BroadcastSnapshot();
}

//-----------------------------------------------------------------------------
// ProcessInputCmd - Handle input from client
// 
// NOTE: Client only sends INPUT INTENT, never position/velocity.
// Server applies movement based on input.
//-----------------------------------------------------------------------------
void MockServer::ProcessInputCmd(const InputCmd& cmd)
{
    uint32_t prevButtons = m_PrevButtons;
    m_PrevButtons = cmd.buttons;
    uint32_t newlyPressed = cmd.buttons & ~prevButtons;  // 0→1 edges
    m_LastInputCmd = cmd;

    // Store camera angles
    m_PlayerState.yaw = cmd.yaw;
    m_PlayerState.pitch = cmd.pitch;

    // Dead players: camera only, skip all actions (mirrors GameServer)
    if (m_PlayerState.stateFlags & NetStateFlags::IS_DEAD)
        return;

    // Update state flags based on buttons
    uint32_t flags = m_PlayerState.stateFlags;

    if (cmd.buttons & InputButtons::FIRE)
        flags |= NetStateFlags::IS_FIRING;
    else
        flags &= ~NetStateFlags::IS_FIRING;

    if (cmd.buttons & InputButtons::ADS)
        flags |= NetStateFlags::IS_ADS;
    else
        flags &= ~NetStateFlags::IS_ADS;

    if (cmd.buttons & InputButtons::RELOAD)
    {
        if (m_ReloadTimer <= 0.0 && m_Ammo < WeaponConfig::MAG_SIZE && m_AmmoReserve > 0)
        {
            if (m_Ammo == 0) {
                m_ReloadTimer = WeaponConfig::RELOAD_OUT_OF_AMMO_DURATION;
                flags |= NetStateFlags::IS_RELOAD_EMPTY;
            } else {
                m_ReloadTimer = WeaponConfig::RELOAD_DURATION;
            }
            // Set IS_RELOADING immediately so ProcessFiring's gate blocks fire this tick.
            // UpdateReloadTimer already ran for this tick, so this won't be decremented
            // until next tick — giving the reload exactly RELOAD_DURATION before completion.
            flags |= NetStateFlags::IS_RELOADING;
        }
    }

    // Interrupt reload on trigger-edges (FIRE/ADS/SPRINT/JUMP)
    constexpr uint32_t INTERRUPT_MASK =
        InputButtons::FIRE | InputButtons::ADS |
        InputButtons::SPRINT | InputButtons::JUMP;
    if (m_ReloadTimer > 0.0 && (newlyPressed & INTERRUPT_MASK))
    {
        m_ReloadTimer = 0.0;
        flags &= ~NetStateFlags::IS_RELOADING;
        flags &= ~NetStateFlags::IS_RELOAD_EMPTY;
        // No ammo refill on interrupt
    }

    // Inspect: set when INSPECT pressed, clear on any action input
    bool hasActionInput = (cmd.buttons & (InputButtons::FIRE | InputButtons::ADS | InputButtons::RELOAD | InputButtons::JUMP | InputButtons::SPRINT)) != 0
        || fabsf(cmd.moveAxisX) > 0.01f || fabsf(cmd.moveAxisY) > 0.01f;

    if (cmd.buttons & InputButtons::INSPECT)
    {
        flags |= NetStateFlags::IS_INSPECTING;
    }
    else if ((flags & NetStateFlags::IS_INSPECTING) && hasActionInput)
    {
        flags &= ~NetStateFlags::IS_INSPECTING;
    }

    m_PlayerState.stateFlags = flags;
}

//-----------------------------------------------------------------------------
// UpdateReloadTimer - Decrement reload timer once per tick
// MUST be called exactly once per Tick(), BEFORE InputCmd processing — this
// ensures reloads started in ProcessInputCmd last exactly RELOAD_DURATION
// (no same-tick decrement) and matches ProcessFiring's auto-reload duration.
//-----------------------------------------------------------------------------
void MockServer::UpdateReloadTimer()
{
    uint32_t& flags = m_PlayerState.stateFlags;

    if (m_ReloadTimer > 0.0)
    {
        flags |= NetStateFlags::IS_RELOADING;
        m_ReloadTimer -= TICK_DURATION;
        if (m_ReloadTimer <= 0.0)
        {
            m_ReloadTimer = 0.0;
            flags &= ~NetStateFlags::IS_RELOADING;
            flags &= ~NetStateFlags::IS_RELOAD_EMPTY;

            // Refill magazine from reserve
            int needed = WeaponConfig::MAG_SIZE - m_Ammo;
            int refill = (m_AmmoReserve >= needed) ? needed : m_AmmoReserve;
            m_Ammo += static_cast<uint8_t>(refill);
            m_AmmoReserve -= static_cast<uint8_t>(refill);
        }
    }
    else
    {
        flags &= ~NetStateFlags::IS_RELOADING;
    }
}

//-----------------------------------------------------------------------------
// SimulatePhysics - Server-side authoritative physics simulation
// 
// CS:GO / Valorant Style Movement:
//   - Ground: Snappy, instant response, no sliding
//   - Air: Momentum preservation, limited air control
//   - Target velocity approach instead of force accumulation
//-----------------------------------------------------------------------------
void MockServer::SimulatePhysics()
{
    const float dt = static_cast<float>(TICK_DURATION);
    
    // ========================================================================
    // MOVEMENT PARAMETERS (CS:GO / Valorant style)
    // ========================================================================
    constexpr float MAX_WALK_SPEED = 5.0f;    // Walking speed
    constexpr float MAX_RUN_SPEED  = 8.0f;    // Sprinting speed
    constexpr float GROUND_ACCEL   = 50.0f;   // High = snappy ground control
    constexpr float AIR_ACCEL      = 2.0f;    // Low = limited air control
    constexpr float GRAVITY        = 20.0f;   // Heavy, quick jumps
    constexpr float JUMP_VELOCITY  = 8.0f;    // Jump impulse
    
    bool isGrounded = (m_PlayerState.stateFlags & NetStateFlags::IS_GROUNDED) != 0;
    bool wasGroundedAtStart = isGrounded;
    
    // ========================================================================
    // STEP 1: Calculate Target Velocity from Input
    // ========================================================================
    float yaw = m_LastInputCmd.yaw;
    
    // Forward/Right vectors from yaw (flattened)
    float frontX = sinf(yaw);
    float frontZ = cosf(yaw);
    float rightX = frontZ;
    float rightZ = -frontX;
    
    // Calculate move direction from input
    float moveX = m_LastInputCmd.moveAxisX * rightX + m_LastInputCmd.moveAxisY * frontX;
    float moveZ = m_LastInputCmd.moveAxisX * rightZ + m_LastInputCmd.moveAxisY * frontZ;
    
    // Normalize diagonal movement
    float moveMag = sqrtf(moveX * moveX + moveZ * moveZ);
    if (moveMag > 1.0f)
    {
        moveX /= moveMag;
        moveZ /= moveMag;
        moveMag = 1.0f;
    }
    
    // Target speed based on sprint
    float maxSpeed = (m_LastInputCmd.buttons & InputButtons::SPRINT) ? MAX_RUN_SPEED : MAX_WALK_SPEED;
    
    // Target velocity = normalized direction * max speed
    float targetVelX = moveX * maxSpeed;
    float targetVelZ = moveZ * maxSpeed;
    
    // ========================================================================
    // STEP 2: Ground vs Air Movement
    // ========================================================================
    if (isGrounded)
    {
        // ---------------------------------------------------------------------
        // GROUND: Snappy, high acceleration, instant stop
        // MoveTowards formula: vel = vel + clamp(target - vel, -accel*dt, accel*dt)
        // ---------------------------------------------------------------------
        float accelStep = GROUND_ACCEL * dt;
        
        // X axis
        float diffX = targetVelX - m_PlayerState.velocity.x;
        if (fabsf(diffX) <= accelStep)
            m_PlayerState.velocity.x = targetVelX;
        else
            m_PlayerState.velocity.x += (diffX > 0 ? accelStep : -accelStep);
        
        // Z axis
        float diffZ = targetVelZ - m_PlayerState.velocity.z;
        if (fabsf(diffZ) <= accelStep)
            m_PlayerState.velocity.z = targetVelZ;
        else
            m_PlayerState.velocity.z += (diffZ > 0 ? accelStep : -accelStep);
        
        // ---------------------------------------------------------------------
        // JUMP - Only on ground
        // ---------------------------------------------------------------------
        if (m_LastInputCmd.buttons & InputButtons::JUMP)
        {
            m_PlayerState.velocity.y = JUMP_VELOCITY;
            m_PlayerState.stateFlags &= ~NetStateFlags::IS_GROUNDED;
            m_PlayerState.stateFlags |= NetStateFlags::IS_JUMPING;
            isGrounded = false;
        }
    }
    else
    {
        // ---------------------------------------------------------------------
        // AIR: Preserve momentum, limited air control (air strafing)
        // No friction applied - only slight influence from input
        // ---------------------------------------------------------------------
        float airStep = AIR_ACCEL * dt;
        
        // Only apply air control if there's input
        if (moveMag > 0.01f)
        {
            // Air strafe: add small acceleration in input direction
            m_PlayerState.velocity.x += moveX * airStep;
            m_PlayerState.velocity.z += moveZ * airStep;
            
            // Cap horizontal speed to prevent infinite acceleration
            float horizSpeed = sqrtf(m_PlayerState.velocity.x * m_PlayerState.velocity.x + 
                                     m_PlayerState.velocity.z * m_PlayerState.velocity.z);
            // Allow slight overspeed from bunny hop
            const float airCap = maxSpeed * PhysicsConfig::AIR_STRAFE_SPEED_MULT;
            if (horizSpeed > airCap)
            {
                float scale = airCap / horizSpeed;
                m_PlayerState.velocity.x *= scale;
                m_PlayerState.velocity.z *= scale;
            }
        }
        // NO friction in air - momentum preserved
    }
    
    // ========================================================================
    // STEP 3: Gravity (always applies, even briefly on ground for stability)
    // ========================================================================
    if (!wasGroundedAtStart)
    {
        m_PlayerState.velocity.y -= GRAVITY * dt;
    }
    
    // ========================================================================
    // STEP 4: Apply velocity to position
    // ========================================================================
    m_PlayerState.position.x += m_PlayerState.velocity.x * dt;
    m_PlayerState.position.z += m_PlayerState.velocity.z * dt;
    m_PlayerState.position.y += m_PlayerState.velocity.y * dt;
    
    // ========================================================================
    // STEP 5: Collision Detection (Capsule vs World AABBs)
    // ========================================================================
    if (m_pCollisionWorld)
    {
        auto result = m_pCollisionWorld->ResolveCapsule(
            m_PlayerState.position, PLAYER_HEIGHT, CAPSULE_RADIUS,
            m_PlayerState.velocity);
        m_PlayerState.position = result.position;
        m_PlayerState.velocity = result.velocity;
        if (result.isGrounded)
        {
            m_PlayerState.stateFlags |= NetStateFlags::IS_GROUNDED;
            m_PlayerState.stateFlags &= ~NetStateFlags::IS_JUMPING;
        }
        else
        {
            m_PlayerState.stateFlags &= ~NetStateFlags::IS_GROUNDED;
        }
    }
    else
    {
        // Fallback: simple floor at y=0
        if (m_PlayerState.position.y <= 0.0f)
        {
            m_PlayerState.position.y = 0.0f;
            m_PlayerState.velocity.y = 0.0f;
            m_PlayerState.stateFlags |= NetStateFlags::IS_GROUNDED;
            m_PlayerState.stateFlags &= ~NetStateFlags::IS_JUMPING;
        }
    }
}

//-----------------------------------------------------------------------------
// Ray-Sphere intersection helper (returns entry distance)
//-----------------------------------------------------------------------------
static bool RaySphere(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir,
                      const DirectX::XMFLOAT3& center, float radius, float& outT)
{
    float ocx = origin.x - center.x;
    float ocy = origin.y - center.y;
    float ocz = origin.z - center.z;
    float a = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    float h = ocx * dir.x + ocy * dir.y + ocz * dir.z;
    float c = ocx * ocx + ocy * ocy + ocz * ocz - radius * radius;
    float disc = h * h - a * c;
    if (disc < 0.0f) return false;
    float sqrtDisc = sqrtf(disc);
    float t = (-h - sqrtDisc) / a;
    if (t < 0.0f) t = (-h + sqrtDisc) / a;
    if (t < 0.0f) return false;
    outT = t;
    return true;
}

//-----------------------------------------------------------------------------
// Ray-Capsule intersection (cylinder + two hemispheres)
//-----------------------------------------------------------------------------
static bool RayCapsule(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir,
                       const DirectX::XMFLOAT3& capBottom, float capHeight, float capRadius,
                       float& outT)
{
    constexpr float MAX_RANGE = 200.0f;

    // Segment endpoints (sphere centers)
    DirectX::XMFLOAT3 segA = { capBottom.x, capBottom.y + capRadius, capBottom.z };
    DirectX::XMFLOAT3 segB = { capBottom.x, capBottom.y + capHeight - capRadius, capBottom.z };
    float segDirY = segB.y - segA.y;
    float segLenSq = segDirY * segDirY; // axis is vertical

    float bestT = MAX_RANGE + 1.0f;
    bool hasHit = false;

    // 1. Infinite cylinder clamped to segment extent (vertical axis)
    if (segLenSq > 1e-8f)
    {
        float segLen = sqrtf(segLenSq);
        // Axis is (0, 1, 0) since capsule is vertical
        float dDotAxis = dir.y;
        float ocx = origin.x - segA.x;
        float ocy = origin.y - segA.y;
        float ocz = origin.z - segA.z;
        float ocDotAxis = ocy;

        // Project out axis component
        float dPerpX = dir.x, dPerpY = dir.y - dDotAxis, dPerpZ = dir.z;
        float ocPerpX = ocx, ocPerpY = ocy - ocDotAxis, ocPerpZ = ocz;

        float a = dPerpX * dPerpX + dPerpY * dPerpY + dPerpZ * dPerpZ;
        float b = dPerpX * ocPerpX + dPerpY * ocPerpY + dPerpZ * ocPerpZ;
        float c = ocPerpX * ocPerpX + ocPerpY * ocPerpY + ocPerpZ * ocPerpZ - capRadius * capRadius;

        float disc = b * b - a * c;
        if (disc >= 0.0f && a > 1e-8f)
        {
            float sqrtDisc = sqrtf(disc);
            float t = (-b - sqrtDisc) / a;
            if (t < 0.0f) t = (-b + sqrtDisc) / a;
            if (t >= 0.0f && t <= MAX_RANGE)
            {
                float hitOnAxis = ocDotAxis + t * dDotAxis;
                if (hitOnAxis >= 0.0f && hitOnAxis <= segLen)
                {
                    bestT = t;
                    hasHit = true;
                }
            }
        }
    }

    // 2. Bottom hemisphere
    float tSphere;
    if (RaySphere(origin, dir, segA, capRadius, tSphere))
    {
        if (tSphere <= MAX_RANGE && tSphere < bestT)
        {
            bestT = tSphere;
            hasHit = true;
        }
    }

    // 3. Top hemisphere
    if (RaySphere(origin, dir, segB, capRadius, tSphere))
    {
        if (tSphere <= MAX_RANGE && tSphere < bestT)
        {
            bestT = tSphere;
            hasHit = true;
        }
    }

    if (hasHit) { outT = bestT; return true; }
    return false;
}

//-----------------------------------------------------------------------------
// Ray-AABB intersection (slab method)
//-----------------------------------------------------------------------------
static bool RayAABB(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir,
                    const DirectX::XMFLOAT3& aabbMin, const DirectX::XMFLOAT3& aabbMax,
                    float& outT)
{
    constexpr float MAX_RANGE = 200.0f;
    float tMin = 0.0f;
    float tMax = MAX_RANGE;

    auto slabTest = [&](float o, float d, float lo, float hi) -> bool {
        if (fabsf(d) < 1e-8f)
            return (o >= lo && o <= hi);
        float inv = 1.0f / d;
        float t1 = (lo - o) * inv;
        float t2 = (hi - o) * inv;
        if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tMin) tMin = t1;
        if (t2 < tMax) tMax = t2;
        return tMin <= tMax;
    };

    if (!slabTest(origin.x, dir.x, aabbMin.x, aabbMax.x)) return false;
    if (!slabTest(origin.y, dir.y, aabbMin.y, aabbMax.y)) return false;
    if (!slabTest(origin.z, dir.z, aabbMin.z, aabbMax.z)) return false;

    outT = tMin;
    return true;
}

//-----------------------------------------------------------------------------
// ProcessFiring - Fire-rate gating + hitscan against remote bot
//-----------------------------------------------------------------------------
void MockServer::ProcessFiring()
{
    // Block firing while reloading or inspecting
    if (m_PlayerState.stateFlags & (NetStateFlags::IS_RELOADING | NetStateFlags::IS_INSPECTING))
    {
        m_FireTimer = 0.0;
        return;
    }

    bool isFiring = (m_LastInputCmd.buttons & InputButtons::FIRE) != 0;

    if (!isFiring)
    {
        m_FireTimer = 0.0;
        return;
    }

    double fireInterval = 60.0 / RED_RPM;

    bool shouldFire = false;
    if (m_FireTimer <= 0.0)
    {
        shouldFire = true;
        m_FireTimer = fireInterval;
    }
    else
    {
        m_FireTimer -= TICK_DURATION;
        if (m_FireTimer <= 0.0)
        {
            shouldFire = true;
            m_FireTimer += fireInterval;
        }
    }

    if (!shouldFire) return;

    if (m_Ammo == 0)
    {
        // Auto-reload: handles empty-fire after interrupted reload, or held FIRE on empty mag
        if (m_AmmoReserve > 0 && m_ReloadTimer <= 0.0) {
            m_ReloadTimer = WeaponConfig::RELOAD_OUT_OF_AMMO_DURATION;
            m_PlayerState.stateFlags |= NetStateFlags::IS_RELOADING;
            m_PlayerState.stateFlags |= NetStateFlags::IS_RELOAD_EMPTY;
        }
        return;
    }

    m_Ammo--;
    m_FireCounter++;

    // Auto-reload IMMEDIATELY when last bullet just fired
    if (m_Ammo == 0 && m_AmmoReserve > 0 && m_ReloadTimer <= 0.0) {
        m_ReloadTimer = WeaponConfig::RELOAD_OUT_OF_AMMO_DURATION;
        m_PlayerState.stateFlags |= NetStateFlags::IS_RELOADING;
        m_PlayerState.stateFlags |= NetStateFlags::IS_RELOAD_EMPTY;
    }

    // Eye position
    DirectX::XMFLOAT3 eyePos = {
        m_PlayerState.position.x,
        m_PlayerState.position.y + 1.5f,
        m_PlayerState.position.z
    };

    // Ray direction from yaw/pitch
    float cosPitch = cosf(m_PlayerState.pitch);
    DirectX::XMFLOAT3 rayDir = {
        sinf(m_PlayerState.yaw) * cosPitch,
        sinf(m_PlayerState.pitch),
        cosf(m_PlayerState.yaw) * cosPitch
    };

    // Lag compensation: clamp the client-reported view tick (same rules as
    // GameServer::ProcessFiring) and rewind the bot capsule to the time the
    // client actually SAW it.
    uint32_t viewTick = m_LastInputCmd.viewTick;
    float viewFrac = m_LastInputCmd.viewTickFrac;
    if (viewTick != 0)
    {
        const uint32_t minTick = (m_CurrentTick > LagCompConfig::MAX_REWIND_TICKS)
            ? m_CurrentTick - LagCompConfig::MAX_REWIND_TICKS : 1u;
        if (viewTick < minTick) viewTick = minTick;
        if (viewTick > m_CurrentTick)
        {
            viewTick = m_CurrentTick;
            viewFrac = 0.0f;  // never extrapolate forward server-side
        }
        if (!(viewFrac >= 0.0f && viewFrac < 1.0f)) viewFrac = 0.0f;  // NaN-safe
    }

    // Nearest wall first: a bot hit only counts if it is in front of the wall
    // the shot would otherwise strike (static geometry is NOT rewound).
    float wallDist = 99999.0f;
    if (m_pCollisionWorld)
    {
        for (const auto& col : m_pCollisionWorld->GetColliders())
        {
            float t = 0.0f;
            if (RayAABB(eyePos, rayDir, col.aabb.min, col.aabb.max, t) && t < wallDist)
                wallDist = t;
        }
    }

    // Test every live bot at its rewound position; keep the closest hit that is
    // also nearer than the wall.
    int   hitBot   = -1;
    float bestDist = wallDist;
    for (int i = 0; i < NUM_BOTS; i++)
    {
        const Bot& b = m_Bots[i];
        if (b.state.stateFlags & NetStateFlags::IS_DEAD)
            continue;
        if (b.team == LOCAL_PLAYER_TEAM)
            continue;  // friendly fire off — the RED player only damages BLUE bots

        DirectX::XMFLOAT3 botPos;
        if (!GetRewoundBotPosition(b, viewTick, viewFrac, botPos))
            continue;  // bot was dead at the time the client saw — no ghost hit

        float d = 0.0f;
        if (RayCapsule(eyePos, rayDir, botPos, PLAYER_HEIGHT, CAPSULE_RADIUS, d)
            && d < bestDist)
        {
            bestDist = d;
            hitBot   = i;
        }
    }

    if (viewTick != 0)
    {
        const double rewindTicks =
            static_cast<double>(m_CurrentTick - viewTick) - viewFrac;
        DebugLog_Printf("LAGCOMP",
            "view=%u+%.2f rewind=%.1ft (%.0fms) hit=%s id=%d dist=%.2f",
            viewTick, viewFrac,
            rewindTicks, rewindTicks * TICK_DURATION * 1000.0,
            hitBot >= 0 ? "yes" : "no",
            hitBot >= 0 ? hitBot + 1 : 0,
            hitBot >= 0 ? bestDist : 0.0f);
    }

    if (hitBot >= 0)
    {
        DamageBot(m_Bots[hitBot], RED_DAMAGE);
        // Hit marker for local player (carries the bot id it struck)
        m_PlayerState.hitByPlayerId = static_cast<uint8_t>(hitBot + 1);
    }
}

//-----------------------------------------------------------------------------
// PaceBot - the shared bot motion: slide left/right along X with a sinusoid
// around a fixed base point. Every remote bot uses this, so they all move
// identically (the combat behaviour layered on top differs only in state).
//-----------------------------------------------------------------------------
static void PaceBot(NetPlayerState& s, const DirectX::XMFLOAT3& base,
                    float serverTime, float phase)
{
    constexpr float w   = 0.6f;   // angular speed (rad/s)
    constexpr float amp = 1.5f;   // pace amplitude (m), kept small to avoid overlap
    const float a = serverTime * w + phase;
    s.position = { base.x + sinf(a) * amp, base.y, base.z };
    s.velocity = { cosf(a) * amp * w, 0.0f, 0.0f };
}

//-----------------------------------------------------------------------------
// UpdateBots - Advance every remote bot one tick.
//
// Each live bot moves with the shared display-bot pace (PaceBot), runs its
// intermittent-fire / auto-reload AI (UpdateBotWeapon), then publishes ammo +
// tick into its wire state. Dead bots hold still and count down to respawn.
// Movement is identical to before — only the combat/fire behaviour is new.
//
// NOTE: the pace peaks at amp*w = 0.9 m/s, so the 100ms interpolation delay
// displaces a rendered bot only ~0.09m — inside the 0.3m capsule radius. The
// server-side rewind below still runs (so fast-moving bots stay hittable), but
// at this speed it is no longer required to land tracking shots.
//-----------------------------------------------------------------------------
void MockServer::UpdateBots()
{
    const float t = static_cast<float>(m_ServerTime);

    for (int i = 0; i < NUM_BOTS; i++)
    {
        Bot& b = m_Bots[i];

        if (b.state.stateFlags & NetStateFlags::IS_DEAD)
        {
            b.respawnTimer -= TICK_DURATION;
            if (b.respawnTimer <= 0.0)
                RespawnBot(b, i);
            b.state.tickId = m_CurrentTick;
            continue;  // no move / no fire on the tick it is dead or just respawned
        }

        PaceBot(b.state, b.base, t, b.phase);
        UpdateBotWeapon(b, i);

        b.state.ammo        = b.ammo;
        b.state.ammoReserve = WeaponConfig::MAX_RESERVE;  // bots never run dry
        b.state.tickId      = m_CurrentTick;
    }
}

//-----------------------------------------------------------------------------
// UpdateBotWeapon - Intermittent-fire AI for one bot.
//
// Fires short bursts (BOT_BURST_TIME) separated by gaps (BOT_GAP_TIME), burning
// one round per shot at BOT_FIRE_RPM. When the magazine empties it auto-reloads
// (out-of-ammo timing) and refills. Only the IS_FIRING / IS_RELOADING flags
// matter to the client — its remote state machine paces muzzle flashes itself.
//-----------------------------------------------------------------------------
void MockServer::UpdateBotWeapon(Bot& b, int index)
{
    uint32_t& flags = b.state.stateFlags;

    // Reloading takes priority and blocks firing.
    if (b.reloadTimer > 0.0)
    {
        flags |= NetStateFlags::IS_RELOADING;   // IS_RELOAD_EMPTY set when it began
        flags &= ~NetStateFlags::IS_FIRING;
        b.reloadTimer -= TICK_DURATION;
        if (b.reloadTimer <= 0.0)
        {
            b.reloadTimer = 0.0;
            b.ammo = WeaponConfig::MAG_SIZE;
            flags &= ~(NetStateFlags::IS_RELOADING | NetStateFlags::IS_RELOAD_EMPTY);
            b.firing = false;
            b.burstTimer = BOT_GAP_TIME;        // short pause before resuming
        }
        return;
    }

    b.burstTimer -= TICK_DURATION;

    if (b.firing)
    {
        flags |= NetStateFlags::IS_FIRING;

        b.fireTimer -= TICK_DURATION;
        if (b.fireTimer <= 0.0)
        {
            b.fireTimer += 60.0 / BOT_FIRE_RPM;
            if (b.ammo > 0)
            {
                b.ammo--;
                b.state.fireCounter++;
                BotFireShot(b, index);   // live hitscan: can damage bots / player
            }
            if (b.ammo == 0)
            {
                // Magazine emptied -> auto-reload (out-of-ammo timing).
                b.reloadTimer = WeaponConfig::RELOAD_OUT_OF_AMMO_DURATION;
                flags |= NetStateFlags::IS_RELOADING | NetStateFlags::IS_RELOAD_EMPTY;
                flags &= ~NetStateFlags::IS_FIRING;
                b.firing = false;
                return;
            }
        }

        if (b.burstTimer <= 0.0)
        {
            // Burst over -> gap.
            b.firing = false;
            flags &= ~NetStateFlags::IS_FIRING;
            b.burstTimer = BOT_GAP_TIME;
        }
    }
    else
    {
        flags &= ~NetStateFlags::IS_FIRING;
        if (b.burstTimer <= 0.0)
        {
            // Gap over -> start a new burst (first shot fires next tick).
            b.firing = true;
            b.fireTimer = 0.0;
            b.burstTimer = BOT_BURST_TIME;
        }
    }
}

//-----------------------------------------------------------------------------
// RespawnBot - Reset a bot to its spawn after the respawn delay.
//-----------------------------------------------------------------------------
void MockServer::RespawnBot(Bot& b, int index)
{
    NetPlayerState& s = b.state;
    s.health = MAX_HEALTH;
    s.position = b.base;
    s.velocity = { 0.0f, 0.0f, 0.0f };
    s.stateFlags &= ~(NetStateFlags::IS_DEAD | NetStateFlags::IS_FIRING |
                      NetStateFlags::IS_RELOADING | NetStateFlags::IS_RELOAD_EMPTY);
    s.stateFlags |= NetStateFlags::IS_GROUNDED;

    b.respawnTimer = 0.0;
    b.ammo = WeaponConfig::MAG_SIZE;
    b.reloadTimer = 0.0;
    b.fireTimer = 0.0;
    b.firing = false;
    b.burstTimer = BOT_GAP_TIME + 0.2 * index;  // re-stagger
    s.ammo = b.ammo;
    s.ammoReserve = WeaponConfig::MAX_RESERVE;
}

//-----------------------------------------------------------------------------
// DamageBot - Apply damage to a bot; kill + start its respawn on lethal.
//-----------------------------------------------------------------------------
void MockServer::DamageBot(Bot& b, uint8_t dmg)
{
    if (b.state.health > dmg)
    {
        b.state.health -= dmg;
        return;
    }

    b.state.health = 0;
    b.state.stateFlags |= NetStateFlags::IS_DEAD;
    b.state.stateFlags &= ~(NetStateFlags::IS_FIRING |
                            NetStateFlags::IS_RELOADING |
                            NetStateFlags::IS_RELOAD_EMPTY);
    b.state.velocity = { 0.0f, 0.0f, 0.0f };
    b.respawnTimer = RESPAWN_TIME;
    b.firing = false;
    b.reloadTimer = 0.0;
}

//-----------------------------------------------------------------------------
// DamagePlayer - Apply bot damage to the local player; kill + start respawn on
// lethal. Health / IS_DEAD propagate via the snapshot; the client handles the
// death fade and the respawn snap-to-spawn.
//-----------------------------------------------------------------------------
void MockServer::DamagePlayer(uint8_t dmg)
{
    uint32_t& flags = m_PlayerState.stateFlags;
    if (flags & NetStateFlags::IS_DEAD)
        return;

    if (m_PlayerState.health > dmg)
    {
        m_PlayerState.health -= dmg;
        return;
    }

    m_PlayerState.health = 0;
    flags |= NetStateFlags::IS_DEAD;
    flags &= ~(NetStateFlags::IS_FIRING | NetStateFlags::IS_RELOADING |
               NetStateFlags::IS_RELOAD_EMPTY | NetStateFlags::IS_INSPECTING);
    m_PlayerState.velocity = { 0.0f, 0.0f, 0.0f };
    m_PlayerRespawnTimer = RESPAWN_TIME;
    m_ReloadTimer = 0.0;
}

//-----------------------------------------------------------------------------
// BotFireShot - One hitscan shot from a bot along its facing (pitch 0). Damages
// the closest live entity — another bot or the local player — that is also in
// front of the nearest wall. Bots fire straight ahead and never seek a target;
// because RED bots face the BLUE row and vice-versa, this alone produces
// RED<->BLUE crossfire and lets BLUE bots hit the player, with no team logic.
//-----------------------------------------------------------------------------
void MockServer::BotFireShot(const Bot& shooter, int shooterIndex)
{
    const DirectX::XMFLOAT3 eye = {
        shooter.state.position.x,
        shooter.state.position.y + BOT_EYE_HEIGHT,
        shooter.state.position.z
    };
    const float yaw = shooter.state.yaw;
    const DirectX::XMFLOAT3 dir = { sinf(yaw), 0.0f, cosf(yaw) };

    // Static geometry occludes shots (not rewound — bots see live positions).
    float bestDist = 99999.0f;
    if (m_pCollisionWorld)
    {
        for (const auto& col : m_pCollisionWorld->GetColliders())
        {
            float t = 0.0f;
            if (RayAABB(eye, dir, col.aabb.min, col.aabb.max, t) && t < bestDist)
                bestDist = t;
        }
    }

    int  hitBot    = -1;
    bool hitPlayer = false;

    // Other live ENEMY bots (live positions — no lag comp between server
    // entities). Same-team bots are skipped entirely: friendly fire passes
    // through them (no damage, no blocking), matching GameServer::RaycastPlayers.
    for (int j = 0; j < NUM_BOTS; j++)
    {
        if (j == shooterIndex) continue;
        const Bot& tb = m_Bots[j];
        if (tb.state.stateFlags & NetStateFlags::IS_DEAD) continue;
        if (tb.team == shooter.team) continue;  // friendly fire off

        float d = 0.0f;
        if (RayCapsule(eye, dir, tb.state.position, PLAYER_HEIGHT, CAPSULE_RADIUS, d)
            && d < bestDist)
        {
            bestDist = d; hitBot = j; hitPlayer = false;
        }
    }

    // Local player (RED) — only enemy (non-RED) bots can hit it.
    if (shooter.team != LOCAL_PLAYER_TEAM &&
        (m_PlayerState.stateFlags & NetStateFlags::IS_DEAD) == 0)
    {
        float d = 0.0f;
        if (RayCapsule(eye, dir, m_PlayerState.position, PLAYER_HEIGHT, CAPSULE_RADIUS, d)
            && d < bestDist)
        {
            bestDist = d; hitBot = -1; hitPlayer = true;
        }
    }

    if (hitPlayer)
        DamagePlayer(BOT_DAMAGE);
    else if (hitBot >= 0)
        DamageBot(m_Bots[hitBot], BOT_DAMAGE);
}

//-----------------------------------------------------------------------------
// UpdatePlayerRespawn - Count down the local player's respawn timer and revive
// them at the corner spawn (mirrors GameServer). The client snaps to this
// position and faces world center on the IS_DEAD -> alive transition.
//-----------------------------------------------------------------------------
void MockServer::UpdatePlayerRespawn()
{
    m_PlayerRespawnTimer -= TICK_DURATION;
    if (m_PlayerRespawnTimer > 0.0)
        return;

    m_PlayerRespawnTimer = 0.0;
    m_PlayerState.health = MAX_HEALTH;
    m_PlayerState.position = { -7.0f, 0.0f, -7.0f };  // corner spawn (matches Initialize)
    m_PlayerState.velocity = { 0.0f, 0.0f, 0.0f };
    m_PlayerState.stateFlags &= ~(NetStateFlags::IS_DEAD | NetStateFlags::IS_FIRING |
                                  NetStateFlags::IS_RELOADING | NetStateFlags::IS_RELOAD_EMPTY |
                                  NetStateFlags::IS_INSPECTING);
    m_PlayerState.stateFlags |= NetStateFlags::IS_GROUNDED;
    m_Ammo = WeaponConfig::MAG_SIZE;
    m_AmmoReserve = WeaponConfig::MAX_RESERVE;
    m_ReloadTimer = 0.0;
    m_FireTimer = 0.0;
    m_PrevButtons = 0;  // reset edge detection across respawn
}

//-----------------------------------------------------------------------------
// GetRewoundBotPosition - Lag compensation lookup (mirrors
// GameServer::GetRewoundPosition; see that function for the boundary rules:
// history for tick N is written at the END of tick N, so the current tick's
// position is the live state).
//
// Returns false when the bot was dead at the viewed tick (not hittable).
//-----------------------------------------------------------------------------
bool MockServer::GetRewoundBotPosition(const Bot& bot, uint32_t viewTick,
                                       float viewFrac,
                                       DirectX::XMFLOAT3& outPos) const
{
    // 0 = "no data" sentinel; >= current tick = "no lag" → live position
    // (callers skip bots that are dead now)
    if (viewTick == 0 || viewTick >= m_CurrentTick)
    {
        outPos = bot.state.position;
        return true;
    }

    const BotHistoryEntry& a = bot.history[viewTick % BOT_HISTORY_SIZE];
    if (a.tick != viewTick)  // older than the buffer / never written
    {
        outPos = bot.state.position;
        return true;
    }
    if (!a.alive)
        return false;  // client was looking at a dead bot — no ghost hits

    outPos = a.position;
    if (viewFrac <= 0.0f) return true;

    // Endpoint B = entry for viewTick+1, or the live position when viewTick+1
    // is the in-flight current tick (moved this tick, history not yet written)
    DirectX::XMFLOAT3 posB{};
    bool haveB = false;
    if (viewTick + 1 == m_CurrentTick)
    {
        posB = bot.state.position;
        haveB = true;
    }
    else
    {
        const BotHistoryEntry& b = bot.history[(viewTick + 1) % BOT_HISTORY_SIZE];
        if (b.tick == viewTick + 1 && b.alive)
        {
            posB = b.position;
            haveB = true;
        }
    }
    if (!haveB) return true;

    // Don't lerp across a respawn teleport — the midpoint never existed
    const float dx = posB.x - outPos.x;
    const float dy = posB.y - outPos.y;
    const float dz = posB.z - outPos.z;
    if (dx * dx + dy * dy + dz * dz >
        REWIND_TELEPORT_GUARD * REWIND_TELEPORT_GUARD)
        return true;

    outPos.x += dx * viewFrac;
    outPos.y += dy * viewFrac;
    outPos.z += dz * viewFrac;
    return true;
}

//-----------------------------------------------------------------------------
// BroadcastSnapshot - Send authoritative state to client
//-----------------------------------------------------------------------------
void MockServer::BroadcastSnapshot()
{
    if (!m_pNetwork) return;

    Snapshot snapshot = {};
    snapshot.tickId = m_CurrentTick;
    snapshot.serverTime = m_ServerTime;
    snapshot.localPlayer = m_PlayerState;
    snapshot.localPlayerId = 0;
    snapshot.localPlayerTeam = LOCAL_PLAYER_TEAM;

    // All remote bots (ids 1..NUM_BOTS). NUM_BOTS == MAX_PLAYERS - 1, so the
    // remotePlayers[] array holds them exactly.
    for (int i = 0; i < NUM_BOTS; i++)
    {
        snapshot.remotePlayers[i].playerId = static_cast<uint8_t>(i + 1);
        snapshot.remotePlayers[i].teamId   = m_Bots[i].team;
        snapshot.remotePlayers[i].state    = m_Bots[i].state;
    }
    snapshot.remotePlayerCount = static_cast<uint8_t>(NUM_BOTS);

    m_pNetwork->SendSnapshot(snapshot);
}

