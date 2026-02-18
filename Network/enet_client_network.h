#pragma once
//=============================================================================
// enet_client_network.h
//
// ENet-based client network implementation.
// Connects to a remote game server and exchanges InputCmd/Snapshot packets.
//=============================================================================

#include "i_network.h"
#include <queue>
#include <mutex>
#include <string>

// Forward declarations for ENet types to avoid winsock.h / winsock2.h conflict.
// ENet headers are only included in the .cpp file.
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

class ENetClientNetwork : public INetwork
{
public:
    ENetClientNetwork();
    ~ENetClientNetwork() override;

    //-------------------------------------------------------------------------
    // Configuration (call before Initialize)
    //-------------------------------------------------------------------------
    void SetServerAddress(const char* host, uint16_t port);

    //-------------------------------------------------------------------------
    // INetwork interface
    //-------------------------------------------------------------------------
    void Initialize() override;
    void Finalize() override;

    // Client -> Server (Upstream)
    void SendInputCmd(const InputCmd& cmd) override;
    bool ReceiveInputCmd(InputCmd& outCmd) override;      // No-op on client
    size_t GetInputQueueSize() const override;             // Always 0 on client

    // Server -> Client (Downstream)
    void SendSnapshot(const Snapshot& snapshot) override;  // No-op on client
    bool ReceiveSnapshot(Snapshot& outSnapshot) override;
    size_t GetSnapshotQueueSize() const override;

    // Statistics
    uint32_t GetTotalInputsSent() const override { return m_TotalInputsSent; }
    uint32_t GetTotalSnapshotsSent() const override { return m_TotalSnapshotsReceived; }

    // Network quality
    uint32_t GetRTT() const override;
    uint32_t GetPacketLoss() const override;
    bool IsConnected() const override { return m_IsConnected; }

    //-------------------------------------------------------------------------
    // ENet-specific
    //-------------------------------------------------------------------------
    void PollEvents();

private:
    ENetHost* m_pClient;
    ENetPeer* m_pServerPeer;

    std::string m_ServerHost;
    uint16_t m_ServerPort;
    bool m_IsConnected;

    // Incoming snapshot queue (filled by PollEvents, consumed by ReceiveSnapshot)
    std::queue<Snapshot> m_SnapshotQueue;
    mutable std::mutex m_SnapshotMutex;

    // Statistics
    uint32_t m_TotalInputsSent;
    uint32_t m_TotalSnapshotsReceived;
};
