//=============================================================================
// remote_player.cpp
//
// Implementation of RemotePlayer with snapshot buffer for interpolation.
//
// Sync Strategy: INTERPOLATION + EXTRAPOLATION
//   Priority: Interpolation > Extrapolation > Snap
//=============================================================================

#include "remote_player.h"
#include "shader_3d_ani.h"
#include "direct3d.h"
#include <cmath>
#include <algorithm>

using namespace DirectX;

// Global remote player array (pre-allocated, indexed by playerId)
RemotePlayer g_RemotePlayers[MAX_PLAYERS];
bool g_RemotePlayerActive[MAX_PLAYERS] = {};

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
RemotePlayer::RemotePlayer()
    : m_InterpolationDelay(0.1)    // 100ms interpolation delay
    , m_MaxExtrapolationTime(0.15) // 150ms max extrapolation
    , m_RenderPosition{ 0.0f, 0.0f, 0.0f }
    , m_Velocity{ 0.0f, 0.0f, 0.0f }
    , m_Yaw(0.0f)
    , m_Pitch(0.0f)
    , m_StateFlags(0)
    , m_SyncMode("INIT")
    , m_DebugLerpFactor(0.0f)
    , m_DebugRenderTime(0.0)
    , m_StuckFrameCount(0)
    , m_LastRenderPosition{ 0.0f, 0.0f, 0.0f }
    , m_DebugIsStuck(false)
    , m_IsActive(false)
    , m_TeamId(PlayerTeam::RED)
    , m_Height(2.0f)
    , m_ModelFront{ 0.0f, 0.0f, 1.0f }
    , m_Model(nullptr)
    , m_Animator(nullptr)
    , m_StateMachine(nullptr)
    , m_WeaponModel(nullptr)
{
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
RemotePlayer::~RemotePlayer()
{
    Finalize();
}

//-----------------------------------------------------------------------------
// Initialize
//-----------------------------------------------------------------------------
void RemotePlayer::Initialize(const XMFLOAT3& position)
{
    m_RenderPosition = position;
    m_Velocity = { 0.0f, 0.0f, 0.0f };
    m_Yaw = 0.0f;
    m_Pitch = 0.0f;
    m_StateFlags = NetStateFlags::IS_GROUNDED;
    m_Height = 2.0f;
    m_ModelFront = { 0.0f, 0.0f, 1.0f };
    m_IsActive = true;
    m_SyncMode = "INIT";
    m_SnapshotBuffer.clear();
    
    // Load character model (default based on m_TeamId)
    const char* modelPath = (m_TeamId == PlayerTeam::BLUE)
        ? "resource/model/lpsp_tpc_blue_003.fbx"
        : "resource/model/lpsp_tpc_red_004.fbx";
    m_Model = ModelAni_Load(modelPath);

    if (m_Model)
    {
        m_Animator = new Animator();
        m_Animator->Init(m_Model);
    }

    // Load weapon model based on team
    const char* weaponPath = (m_TeamId == PlayerTeam::BLUE)
        ? "resource/model/m4_003.fbx"
        : "resource/model/ak_002.fbx";
    m_WeaponModel = ModelLoad(weaponPath, 1.0f);

    // Initialize state machine
    m_StateMachine = new RemotePlayerStateMachine();
}

//-----------------------------------------------------------------------------
// Finalize
//-----------------------------------------------------------------------------
void RemotePlayer::Finalize()
{
    m_IsActive = false;
    m_SnapshotBuffer.clear();
    
    if (m_StateMachine)
    {
        delete m_StateMachine;
        m_StateMachine = nullptr;
    }
    
    if (m_Animator)
    {
        delete m_Animator;
        m_Animator = nullptr;
    }
    
    if (m_Model)
    {
        ModelAni_Release(m_Model);
        m_Model = nullptr;
    }

    if (m_WeaponModel)
    {
        ModelRelease(m_WeaponModel);
        m_WeaponModel = nullptr;
    }
}

//-----------------------------------------------------------------------------
// SetTeam - Swap model when team changes
//-----------------------------------------------------------------------------
void RemotePlayer::SetTeam(uint8_t teamId)
{
    if (teamId == m_TeamId) return;
    m_TeamId = teamId;

    // Swap model for new team
    if (m_Animator)
    {
        delete m_Animator;
        m_Animator = nullptr;
    }
    if (m_Model)
    {
        ModelAni_Release(m_Model);
        m_Model = nullptr;
    }

    const char* modelPath = (m_TeamId == PlayerTeam::BLUE)
        ? "resource/model/lpsp_tpc_blue_003.fbx"
        : "resource/model/lpsp_tpc_red_004.fbx";
    m_Model = ModelAni_Load(modelPath);

    if (m_Model)
    {
        m_Animator = new Animator();
        m_Animator->Init(m_Model);
    }

    // Swap weapon model
    if (m_WeaponModel)
    {
        ModelRelease(m_WeaponModel);
        m_WeaponModel = nullptr;
    }
    const char* weaponPath = (m_TeamId == PlayerTeam::BLUE)
        ? "resource/model/m4_003.fbx"
        : "resource/model/ak_002.fbx";
    m_WeaponModel = ModelLoad(weaponPath, 1.0f);
}

//-----------------------------------------------------------------------------
// PushSnapshot - Add new server snapshot to buffer
//-----------------------------------------------------------------------------
void RemotePlayer::PushSnapshot(const NetPlayerState& state, double currentTime)
{
    RemoteSnapshot snapshot;
    snapshot.state = state;
    snapshot.receiveTime = currentTime;
    
    m_SnapshotBuffer.push_back(snapshot);
    
    // Keep buffer size manageable
    while (m_SnapshotBuffer.size() > MAX_BUFFER_SIZE)
    {
        m_SnapshotBuffer.erase(m_SnapshotBuffer.begin());
    }
}

//-----------------------------------------------------------------------------
// Update - Interpolate/Extrapolate render position (called every frame)
//-----------------------------------------------------------------------------
void RemotePlayer::Update(double elapsed_time, double currentTime)
{
    if (!m_IsActive) return;
    
    // Calculate render time (delayed behind real time for interpolation)
    double renderTime = currentTime - m_InterpolationDelay;
    m_DebugRenderTime = renderTime;  // Save for debug
    
    // Save previous position for stuck detection
    DirectX::XMFLOAT3 prevPos = m_RenderPosition;
    
    if (m_SnapshotBuffer.empty())
    {
        m_SyncMode = "NODATA";
        m_DebugLerpFactor = 0.0f;
        return;
    }
    
    // =========================================================================
    // FIXED: Find CONSECUTIVE snapshots where snap[i].time <= renderTime < snap[i+1].time
    // This ensures we always interpolate between adjacent snapshots in time order
    // =========================================================================
    int fromIdx = -1;
    int toIdx = -1;
    
    for (size_t i = 0; i + 1 < m_SnapshotBuffer.size(); ++i)
    {
        double t0 = m_SnapshotBuffer[i].receiveTime;
        double t1 = m_SnapshotBuffer[i + 1].receiveTime;
        
        // Ensure t0 < t1 (snapshots are in correct order)
        if (t0 >= t1) continue;  // Skip invalid/duplicate snapshots
        
        if (t0 <= renderTime && renderTime < t1)
        {
            fromIdx = static_cast<int>(i);
            toIdx = static_cast<int>(i + 1);
            break;
        }
    }
    
    if (fromIdx >= 0 && toIdx >= 0)
    {
        // ===== INTERPOLATION =====
        // We have valid bracketing consecutive snapshots
        m_SyncMode = "INTERP";
        InterpolateBetween(m_SnapshotBuffer[fromIdx], m_SnapshotBuffer[toIdx], renderTime);
    }
    else
    {
        // No bracketing pair found - check edge cases
        const RemoteSnapshot& oldest = m_SnapshotBuffer.front();
        const RemoteSnapshot& newest = m_SnapshotBuffer.back();
        
        if (renderTime < oldest.receiveTime)
        {
            // ===== WAITING =====
            // renderTime is before all snapshots (buffer hasn't caught up)
            m_SyncMode = "WAIT";
            m_RenderPosition = oldest.state.position;
            m_Velocity = oldest.state.velocity;
            m_Yaw = oldest.state.yaw;
            m_Pitch = oldest.state.pitch;
            m_StateFlags = oldest.state.stateFlags;
            m_DebugLerpFactor = 0.0f;
        }
        else if (renderTime >= newest.receiveTime)
        {
            // ===== EXTRAPOLATION =====
            // renderTime is past all snapshots
            double timeSinceNewest = renderTime - newest.receiveTime;
            
            if (timeSinceNewest < m_MaxExtrapolationTime)
            {
                m_SyncMode = "EXTRAP";
                ExtrapolateFrom(newest, timeSinceNewest);
            }
            else
            {
                // ===== SNAP =====
                // Extrapolation limit exceeded
                m_SyncMode = "SNAP";
                m_RenderPosition = newest.state.position;
                m_Velocity = newest.state.velocity;
                m_Yaw = newest.state.yaw;
                m_Pitch = newest.state.pitch;
                m_StateFlags = newest.state.stateFlags;
                m_DebugLerpFactor = 1.0f;
            }
        }
    }
    
    // =========================================================================
    // STUCK DETECTION: If position unchanged for 3+ frames while velocity > 0
    // =========================================================================
    float velMag = sqrtf(m_Velocity.x * m_Velocity.x + m_Velocity.z * m_Velocity.z);
    bool posChanged = (prevPos.x != m_RenderPosition.x || 
                       prevPos.y != m_RenderPosition.y || 
                       prevPos.z != m_RenderPosition.z);
    
    if (!posChanged && velMag > 0.1f)
    {
        m_StuckFrameCount++;
        if (m_StuckFrameCount >= 3)
        {
            m_DebugIsStuck = true;
            // Log warning (would use OutputDebugString in real app)
        }
    }
    else
    {
        m_StuckFrameCount = 0;
        m_DebugIsStuck = false;
    }
    m_LastRenderPosition = m_RenderPosition;
    
    // Update model facing direction
    m_ModelFront.x = sinf(m_Yaw);
    m_ModelFront.y = 0.0f;
    m_ModelFront.z = cosf(m_Yaw);
    
    // Update state machine based on velocity
    bool isGrounded = (m_StateFlags & NetStateFlags::IS_GROUNDED) != 0;
    if (m_StateMachine)
    {
        m_StateMachine->DeriveStateFromVelocity(
            m_Velocity.x, m_Velocity.z, m_Velocity.y, isGrounded, m_Yaw
        );
        
        // Derive weapon state from network state flags
        if (m_StateFlags & NetStateFlags::IS_RELOADING)
        {
            if (m_StateFlags & NetStateFlags::IS_RELOAD_EMPTY)
                m_StateMachine->SetWeaponState(RemoteWeaponState::RELOAD_EMPTY);
            else
                m_StateMachine->SetWeaponState(RemoteWeaponState::RELOAD);
        }
        else if (m_StateFlags & NetStateFlags::IS_FIRING)
        {
            m_StateMachine->SetWeaponState(RemoteWeaponState::FIRING);
        }
        else
        {
            m_StateMachine->SetWeaponState(RemoteWeaponState::NONE);
        }
        
        m_StateMachine->Update(elapsed_time, m_Animator);
    }
    
    // Clean up old snapshots (keep at least 3 for interpolation margin)
    while (m_SnapshotBuffer.size() > 3 && 
           m_SnapshotBuffer.front().receiveTime < renderTime - 0.5)
    {
        m_SnapshotBuffer.erase(m_SnapshotBuffer.begin());
    }
}

//-----------------------------------------------------------------------------
// InterpolateBetween - Lerp between two snapshots
//-----------------------------------------------------------------------------
void RemotePlayer::InterpolateBetween(const RemoteSnapshot& a, const RemoteSnapshot& b, double renderTime)
{
    double duration = b.receiveTime - a.receiveTime;
    if (duration <= 0.0) duration = 0.001;
    
    float t = static_cast<float>((renderTime - a.receiveTime) / duration);
    t = std::max(0.0f, std::min(1.0f, t));  // Clamp 0-1
    m_DebugLerpFactor = t;  // Save for debug
    
    // Lerp position
    m_RenderPosition.x = a.state.position.x + (b.state.position.x - a.state.position.x) * t;
    m_RenderPosition.y = a.state.position.y + (b.state.position.y - a.state.position.y) * t;
    m_RenderPosition.z = a.state.position.z + (b.state.position.z - a.state.position.z) * t;
    
    // Lerp velocity (for animation state)
    m_Velocity.x = a.state.velocity.x + (b.state.velocity.x - a.state.velocity.x) * t;
    m_Velocity.y = a.state.velocity.y + (b.state.velocity.y - a.state.velocity.y) * t;
    m_Velocity.z = a.state.velocity.z + (b.state.velocity.z - a.state.velocity.z) * t;
    
    // Lerp angles (shortest-path for yaw to avoid ±π wrap-around flicker)
    float yawDiff = b.state.yaw - a.state.yaw;
    if (yawDiff > XM_PI)       yawDiff -= XM_2PI;
    else if (yawDiff < -XM_PI) yawDiff += XM_2PI;
    m_Yaw = a.state.yaw + yawDiff * t;

    m_Pitch = a.state.pitch + (b.state.pitch - a.state.pitch) * t;
    
    // Use later snapshot's flags
    m_StateFlags = b.state.stateFlags;
}

//-----------------------------------------------------------------------------
// ExtrapolateFrom - Project position forward using velocity
//-----------------------------------------------------------------------------
void RemotePlayer::ExtrapolateFrom(const RemoteSnapshot& latest, double dt)
{
    float fdt = static_cast<float>(dt);
    
    m_RenderPosition.x = latest.state.position.x + latest.state.velocity.x * fdt;
    m_RenderPosition.y = latest.state.position.y + latest.state.velocity.y * fdt;
    m_RenderPosition.z = latest.state.position.z + latest.state.velocity.z * fdt;
    
    m_Velocity = latest.state.velocity;
    m_Yaw = latest.state.yaw;
    m_Pitch = latest.state.pitch;
    m_StateFlags = latest.state.stateFlags;
}

//-----------------------------------------------------------------------------
// Draw - Render the remote player model
//-----------------------------------------------------------------------------
void RemotePlayer::Draw()
{
    if (!m_IsActive || !m_Model) return;

    XMMATRIX rotation = XMMatrixRotationY(m_Yaw + XM_PI);
    XMMATRIX translation = XMMatrixTranslation(
        m_RenderPosition.x,
        m_RenderPosition.y,
        m_RenderPosition.z
    );
    XMMATRIX world = rotation * translation;

    if (m_Animator)
    {
        ModelAni_Draw(m_Model, world, m_Animator, true);

        // Weapon attachment via "weapon" bone
        if (m_WeaponModel && m_Model->BoneMapping.count("weapon"))
        {
            int boneIdx = m_Model->BoneMapping.at("weapon");
            XMMATRIX weaponBone = XMLoadFloat4x4(&m_Animator->GetBoneGlobalMatrix(boneIdx));

            XMMATRIX preRot  = XMMatrixRotationX(XMConvertToRadians(90.0f));
            XMMATRIX postRot = XMMatrixRotationX(XMConvertToRadians(90.0f));

            // Per-weapon offset (RED=ak, BLUE=m4)
            XMMATRIX offset = (m_TeamId == PlayerTeam::BLUE)
                ? XMMatrixTranslation(0.0f, 0.0f, 0.03f)   // m4
                : XMMatrixTranslation(0.0f, 0.0f, 0.07f);  // ak

            XMMATRIX weaponWorld = offset * postRot * weaponBone * preRot * world;
            ModelDraw(m_WeaponModel, weaponWorld);
        }
    }
}

//-----------------------------------------------------------------------------
// GetEyePosition
//-----------------------------------------------------------------------------
XMFLOAT3 RemotePlayer::GetEyePosition() const
{
    XMFLOAT3 eyePos = m_RenderPosition;
    eyePos.y += m_Height * 0.85f;
    return eyePos;
}

//-----------------------------------------------------------------------------
// State string getters for debug
//-----------------------------------------------------------------------------
std::string RemotePlayer::GetPlayerStateString() const
{
    if (m_StateMachine)
        return m_StateMachine->GetPlayerStateString();
    return "N/A";
}

std::string RemotePlayer::GetWeaponStateString() const
{
    if (m_StateMachine)
        return m_StateMachine->GetWeaponStateString();
    return "N/A";
}

std::string RemotePlayer::GetMoveDirectionString() const
{
    if (m_StateMachine)
        return m_StateMachine->GetMoveDirectionString();
    return "N/A";
}

//-----------------------------------------------------------------------------
// GetOldestSnapshotTime
//-----------------------------------------------------------------------------
double RemotePlayer::GetOldestSnapshotTime() const
{
    if (m_SnapshotBuffer.empty()) return 0.0;
    return m_SnapshotBuffer.front().receiveTime;
}

//-----------------------------------------------------------------------------
// GetNewestSnapshotTime
//-----------------------------------------------------------------------------
double RemotePlayer::GetNewestSnapshotTime() const
{
    if (m_SnapshotBuffer.empty()) return 0.0;
    return m_SnapshotBuffer.back().receiveTime;
}
