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

    // Expected local map checksum for the MAP_INFO handshake (0 = skip check)
    void SetExpectedMapChecksum(uint32_t c) { m_ExpectedMapChecksum = c; }

#if defined(_DEBUG)
    //-------------------------------------------------------------------------
    // Flood debug mode (Debug builds ONLY — an attack tool, never in Release).
    // Blasts packets at the server to stress-test its inbound-flood mitigation
    // from the real client; deliberately bypasses InputProducer's 60Hz send
    // throttle.
    //-------------------------------------------------------------------------
    enum class FloodMode { Valid = 0, Junk = 1, Oversized = 2 };
    void   SetFloodDebug(bool active, int ratePerSec, int mode);
    void   DriveFloodDebug(double deltaTime);   // call once per frame
    bool   IsFloodActive() const           { return m_FloodActive; }
    double GetFloodSendRate() const        { return m_FloodSendRate; }        // pkts/s actually sent
    double GetFloodSnapIntervalMs() const  { return m_FloodSnapIntervalMs; }  // this client's snapshot gap
#endif

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

    // MAP_INFO handshake: checksum of the locally loaded map (0 = don't verify)
    uint32_t m_ExpectedMapChecksum = 0;

#if defined(_DEBUG)
    // Flood debug mode state (Debug builds only).
    bool      m_FloodActive = false;
    int       m_FloodRatePerSec = 1000;
    FloodMode m_FloodMode = FloodMode::Valid;
    double    m_FloodSendAccumulator = 0.0;   // fractional packets carried between frames
    double    m_FloodStatTimer = 0.0;         // ~1s window for the readout
    uint32_t  m_FloodSentThisWindow = 0;
    uint32_t  m_FloodSnapAtWindowStart = 0;
    double    m_FloodSendRate = 0.0;          // pkts/s actually sent (readout)
    double    m_FloodSnapIntervalMs = 0.0;    // this client's own snapshot interval (readout)
#endif
};
