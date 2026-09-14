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

    // Match-room membership (see INetwork). Non-blocking: LeaveSession drops the
    // peer without waiting for an ack, BeginJoinSession only kicks off the
    // handshake, PollEvents drives it, and IsJoinSettled reports when the caller
    // may lift its loading curtain - on success OR on give-up.
    void LeaveSession() override;
    void BeginJoinSession() override;
    bool IsJoinSettled() const override { return m_JoinPhase == JoinPhase::Idle; }

    // Hard ceiling on the undrained snapshot backlog. Only Game_Update drains
    // this, and it runs only in the game scene, so any other scene accumulates
    // forever - the title screen used to bank ~2400 snapshots (1.8 MB) a
    // minute and replay the lot in the single frame after PLAY, marching the
    // player through a match that had already finished. 2s of slack is far more
    // than a frame loop that is keeping up ever needs; past that the oldest is
    // dropped, because a snapshot is a complete world state and the newest is
    // the only one that matters.
    static constexpr size_t MAX_QUEUED_SNAPSHOTS = 64;   // 2s @ 32Hz

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
    // Ask the server for a seat in the match. Sent once the map check passes,
    // never at connect time (see PacketType::JOIN_REQUEST).
    void SendJoinRequest();

    // How far a join attempt has got. Joining is not one round trip: the
    // transport handshake, then the map check (MAP_INFO carries the server's
    // collision checksum - a client on the wrong map must drop out BEFORE it
    // takes a slot in the room), then JOIN_REQUEST, and finally the server's
    // first snapshot, which is its answer. The curtain waits for that answer,
    // not merely for a UDP peer, so gameplay never starts against an empty
    // world.
    enum class JoinPhase : uint8_t {
        Idle,          // nothing in flight - settled
        Connecting,    // waiting for the ENet CONNECT event
        VerifyingMap,  // connected; waiting for MAP_INFO
        AwaitingWorld, // JOIN_REQUEST sent; waiting for the first snapshot
    };

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

    // Join handshake in flight. m_JoinDeadlineMs is an enet_time_get() stamp;
    // PollEvents gives up past it so a dead server cannot pin the caller's
    // loading curtain up forever - it settles disconnected instead, and the
    // player can simply press PLAY again.
    JoinPhase m_JoinPhase = JoinPhase::Idle;
    uint32_t  m_JoinDeadlineMs = 0;

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
