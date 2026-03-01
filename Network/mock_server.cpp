//=============================================================================
// mock_server.cpp
//
// Mock Server implementation with fixed 32Hz tick rate.
// Uses accumulator pattern for frame-rate independent simulation.
//=============================================================================

#include "mock_server.h"
#include "i_network.h"
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

    // Initialize player state (match Player_Fps spawn in Game_Initialize)
    m_PlayerState.tickId = 0;
    m_PlayerState.position = { -7.0f, 0.0f, -7.0f };  // Corner spawn
    m_PlayerState.velocity = { 0.0f, 0.0f, 0.0f };
    m_PlayerState.yaw = 0.0f;
    m_PlayerState.pitch = 0.0f;
    m_PlayerState.stateFlags = NetStateFlags::IS_GROUNDED;
    m_PlayerState.health = 200;
    m_PlayerState.hitByPlayerId = 0xFF;
    m_PlayerState.fireCounter = 0;

    // Initialize last input
    m_LastInputCmd = {};

    // Spawn remote bot player at a different corner
    m_RemotePlayerState = {};
    m_RemotePlayerState.tickId = 0;
    m_RemotePlayerState.position = { 7.0f, 0.0f, 7.0f };
    m_RemotePlayerState.velocity = { 0.0f, 0.0f, 0.0f };
    m_RemotePlayerState.yaw = 0.0f;
    m_RemotePlayerState.pitch = 0.0f;
    m_RemotePlayerState.stateFlags = NetStateFlags::IS_GROUNDED;
    m_RemotePlayerState.health = MAX_HEALTH;
    m_RemotePlayerState.hitByPlayerId = 0xFF;
    m_RemotePlayerState.fireCounter = 0;
    m_RemoteHealth = MAX_HEALTH;
    m_RemoteRespawnTimer = 0.0;
    m_FireTimer = 0.0;
    m_FireCounter = 0;
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
//   1. Consume input commands from client
//   2. Simulate physics
//   3. Update game state
//   4. Broadcast snapshot to client
//-----------------------------------------------------------------------------
void MockServer::Tick()
{
    m_CurrentTick++;
    m_ServerTime += TICK_DURATION;

    // 1. Consume all pending input commands
    InputCmd cmd;
    while (m_pNetwork->ReceiveInputCmd(cmd))
    {
        ProcessInputCmd(cmd);
    }

    // 2. Simulate physics for this tick
    SimulatePhysics();

    // 3. Clear hit marker, then process combat
    m_PlayerState.hitByPlayerId = 0xFF;
    m_RemotePlayerState.hitByPlayerId = 0xFF;

    if (m_RemotePlayerState.stateFlags & NetStateFlags::IS_DEAD)
    {
        // Respawn timer
        m_RemoteRespawnTimer -= TICK_DURATION;
        if (m_RemoteRespawnTimer <= 0.0)
        {
            m_RemoteHealth = MAX_HEALTH;
            m_RemotePlayerState.stateFlags &= ~NetStateFlags::IS_DEAD;
            m_RemotePlayerState.stateFlags |= NetStateFlags::IS_GROUNDED;
            m_RemotePlayerState.position = { 7.0f, 0.0f, 7.0f };
            m_RemotePlayerState.velocity = { 0.0f, 0.0f, 0.0f };
            m_RemoteRespawnTimer = 0.0;
        }
    }
    else
    {
        ProcessFiring();
    }

    m_RemotePlayerState.health = m_RemoteHealth;

    // 4. Update tick ID in states
    m_PlayerState.tickId = m_CurrentTick;
    m_PlayerState.fireCounter = m_FireCounter;

    // 5. Broadcast snapshot to client
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
    m_LastInputCmd = cmd;

    // Store camera angles
    m_PlayerState.yaw = cmd.yaw;
    m_PlayerState.pitch = cmd.pitch;

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
        // Start reload latch — keep IS_RELOADING active for duration
        if (m_ReloadTimer <= 0.0)
            m_ReloadTimer = RELOAD_DURATION;
    }
    
    // Reload latch timer
    if (m_ReloadTimer > 0.0)
    {
        flags |= NetStateFlags::IS_RELOADING;
        m_ReloadTimer -= TICK_DURATION;
        if (m_ReloadTimer <= 0.0)
        {
            m_ReloadTimer = 0.0;
            flags &= ~NetStateFlags::IS_RELOADING;
        }
    }
    else
    {
        flags &= ~NetStateFlags::IS_RELOADING;
    }

    m_PlayerState.stateFlags = flags;
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
            if (horizSpeed > maxSpeed * 1.2f)  // Allow slight overspeed from bunny hop
            {
                float scale = (maxSpeed * 1.2f) / horizSpeed;
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

    m_FireCounter++;

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

    // Test against remote bot capsule
    float hitDist = 0.0f;
    if (RayCapsule(eyePos, rayDir, m_RemotePlayerState.position,
                   PLAYER_HEIGHT, CAPSULE_RADIUS, hitDist))
    {
        // Check if a wall is closer than the player hit
        float wallDist = 99999.0f;
        if (m_pCollisionWorld)
        {
            for (const auto& col : m_pCollisionWorld->GetColliders())
            {
                float t = 0.0f;
                if (RayAABB(eyePos, rayDir, col.aabb.min, col.aabb.max, t))
                {
                    if (t < wallDist) wallDist = t;
                }
            }
        }

        // Only damage if player is closer than the nearest wall
        if (hitDist < wallDist)
        {
            if (m_RemoteHealth > RED_DAMAGE)
            {
                m_RemoteHealth -= RED_DAMAGE;
            }
            else
            {
                m_RemoteHealth = 0;
                m_RemotePlayerState.stateFlags |= NetStateFlags::IS_DEAD;
                m_RemoteRespawnTimer = RESPAWN_TIME;
                m_RemotePlayerState.velocity = { 0.0f, 0.0f, 0.0f };
            }

            // Hit marker for local player
            m_PlayerState.hitByPlayerId = 1;
        }
    }
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
    snapshot.localPlayerTeam = PlayerTeam::RED;

    // Include remote bot player
    m_RemotePlayerState.tickId = m_CurrentTick;
    snapshot.remotePlayers[0].playerId = 1;
    snapshot.remotePlayers[0].teamId = PlayerTeam::BLUE;
    snapshot.remotePlayers[0].state = m_RemotePlayerState;
    snapshot.remotePlayerCount = 1;

    m_pNetwork->SendSnapshot(snapshot);
}

