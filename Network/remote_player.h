#pragma once
//=============================================================================
// remote_player.h
//
// Represents another player in the game world, controlled by server data.
// 
// Sync Strategy: INTERPOLATION + EXTRAPOLATION (with snapshot buffer)
//   - Maintains buffer of recent server snapshots
//   - Render time is delayed by ~100ms for interpolation
//   - Interpolates between snapshots for smooth movement
//   - Extrapolates if no recent data (packet loss)
//   - Snaps if too far behind
//=============================================================================

#include <DirectXMath.h>
#include <vector>
#include "net_common.h"
#include "model_ani.h"
#include "remote_player_state_machine.h"

//-----------------------------------------------------------------------------
// Snapshot for buffering server updates
//-----------------------------------------------------------------------------
struct RemoteSnapshot
{
    NetPlayerState state;
    double receiveTime;     // Local time when received
};

class RemotePlayer
{
public:
    RemotePlayer();
    ~RemotePlayer();

    //-------------------------------------------------------------------------
    // Lifecycle
    //-------------------------------------------------------------------------
    void Initialize(const DirectX::XMFLOAT3& position);
    void Finalize();
    
    //-------------------------------------------------------------------------
    // Add snapshot to buffer (called when server data received)
    //-------------------------------------------------------------------------
    void PushSnapshot(const NetPlayerState& state, double currentTime);
    
    //-------------------------------------------------------------------------
    // Update interpolation/extrapolation (called every frame)
    //-------------------------------------------------------------------------
    void Update(double elapsed_time, double currentTime);
    
    //-------------------------------------------------------------------------
    // Draw the remote player model
    //-------------------------------------------------------------------------
    void Draw();
    
    //-------------------------------------------------------------------------
    // Getters
    //-------------------------------------------------------------------------
    const DirectX::XMFLOAT3& GetRenderPosition() const { return m_RenderPosition; }
    const DirectX::XMFLOAT3& GetVelocity() const { return m_Velocity; }
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }
    bool IsActive() const { return m_IsActive; }
    float GetHeight() const { return m_Height; }
    
    DirectX::XMFLOAT3 GetEyePosition() const;
    
    //-------------------------------------------------------------------------
    // Debug Info
    //-------------------------------------------------------------------------
    const char* GetSyncMode() const { return m_SyncMode; }
    size_t GetBufferSize() const { return m_SnapshotBuffer.size(); }
    double GetInterpolationDelay() const { return m_InterpolationDelay; }
    float GetLerpFactor() const { return m_DebugLerpFactor; }
    double GetLastRenderTime() const { return m_DebugRenderTime; }
    double GetOldestSnapshotTime() const;
    double GetNewestSnapshotTime() const;
    bool IsStuck() const { return m_DebugIsStuck; }
    std::string GetPlayerStateString() const;
    std::string GetWeaponStateString() const;
    std::string GetMoveDirectionString() const;
    
    void SetActive(bool active) { m_IsActive = active; }

private:
    //-------------------------------------------------------------------------
    // Helper: Lerp between two snapshots
    //-------------------------------------------------------------------------
    void InterpolateBetween(const RemoteSnapshot& a, const RemoteSnapshot& b, double renderTime);
    
    //-------------------------------------------------------------------------
    // Helper: Extrapolate from latest snapshot
    //-------------------------------------------------------------------------
    void ExtrapolateFrom(const RemoteSnapshot& latest, double dt);

private:
    // Snapshot buffer for interpolation
    std::vector<RemoteSnapshot> m_SnapshotBuffer;
    static const size_t MAX_BUFFER_SIZE = 32;
    
    // Interpolation parameters
    double m_InterpolationDelay;    // How far behind real-time we render (100ms)
    double m_MaxExtrapolationTime;  // Max time to extrapolate (150ms)
    
    // Render state (what we display)
    DirectX::XMFLOAT3 m_RenderPosition;
    DirectX::XMFLOAT3 m_Velocity;
    float m_Yaw;
    float m_Pitch;
    uint32_t m_StateFlags;
    
    // Debug
    const char* m_SyncMode;
    float m_DebugLerpFactor;
    double m_DebugRenderTime;
    int m_StuckFrameCount;  // Frames where position hasn't changed
    DirectX::XMFLOAT3 m_LastRenderPosition;  // For stuck detection
    bool m_DebugIsStuck;
    
    // Display properties
    bool m_IsActive;
    float m_Height;
    DirectX::XMFLOAT3 m_ModelFront;
    
    // Animation
    MODEL_ANI* m_Model;
    Animator* m_Animator;
    RemotePlayerStateMachine* m_StateMachine;
};

// Global accessor
extern RemotePlayer* g_pRemotePlayer;
