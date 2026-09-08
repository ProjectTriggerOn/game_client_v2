//=============================================================================
// enet_client_network.cpp
//
// ENet-based client network implementation.
//=============================================================================

// WinSock2.h must come before Windows.h to avoid winsock.h conflict
#include <WinSock2.h>
#include <enet/enet.h>
#include <enet/time.h>   // ENET_TIME_GREATER_EQUAL (not pulled in by enet.h)
#include "enet_client_network.h"
#include "net_packet.h"
#include <cstring>
#include <cstdio>
#include <queue>

namespace {
// Budget for a rematch handshake before we give up and let the caller carry on
// disconnected. Matches Initialize()'s blocking wait so both paths fail over on
// the same timescale.
constexpr uint32_t REMATCH_TIMEOUT_MS = 5000;
} // namespace

ENetClientNetwork::ENetClientNetwork()
    : m_pClient(nullptr)
    , m_pServerPeer(nullptr)
    , m_ServerHost("127.0.0.1")
    , m_ServerPort(7777)
    , m_IsConnected(false)
    , m_TotalInputsSent(0)
    , m_TotalSnapshotsReceived(0)
{
}

ENetClientNetwork::~ENetClientNetwork()
{
    Finalize();
}

void ENetClientNetwork::SetServerAddress(const char* host, uint16_t port)
{
    m_ServerHost = host;
    m_ServerPort = port;
}

void ENetClientNetwork::Initialize()
{
    if (enet_initialize() != 0)
    {
        return;
    }

    // Create client host: no incoming connections, 1 outgoing, 2 channels
    m_pClient = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!m_pClient)
    {
        enet_deinitialize();
        return;
    }

    // Resolve server address
    ENetAddress address;
    enet_address_set_host(&address, m_ServerHost.c_str());
    address.port = m_ServerPort;

    // Initiate connection (2 channels, no extra data)
    m_pServerPeer = enet_host_connect(m_pClient, &address, 2, 0);
    if (!m_pServerPeer)
    {
        enet_host_destroy(m_pClient);
        m_pClient = nullptr;
        enet_deinitialize();
        return;
    }

    // Wait up to 5 seconds for the connection to succeed
    ENetEvent event;
    if (enet_host_service(m_pClient, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        m_IsConnected = true;
    }
    else
    {
        // Connection failed - reset peer
        enet_peer_reset(m_pServerPeer);
        m_pServerPeer = nullptr;
    }

    m_TotalInputsSent = 0;
    m_TotalSnapshotsReceived = 0;
}

void ENetClientNetwork::Finalize()
{
    if (m_pServerPeer && m_IsConnected)
    {
        enet_peer_disconnect(m_pServerPeer, 0);

        // Wait up to 3 seconds for graceful disconnect
        ENetEvent event;
        bool disconnected = false;
        while (enet_host_service(m_pClient, &event, 3000) > 0)
        {
            if (event.type == ENET_EVENT_TYPE_DISCONNECT)
            {
                disconnected = true;
                break;
            }
            if (event.type == ENET_EVENT_TYPE_RECEIVE)
            {
                enet_packet_destroy(event.packet);
            }
        }

        if (!disconnected && m_pServerPeer)
        {
            enet_peer_reset(m_pServerPeer);
        }
    }

    m_pServerPeer = nullptr;
    m_IsConnected = false;
    m_Rematching = false;

    if (m_pClient)
    {
        enet_host_destroy(m_pClient);
        m_pClient = nullptr;
    }

    enet_deinitialize();
}

//-----------------------------------------------------------------------------
// PollEvents - Must be called every frame to pump ENet
//-----------------------------------------------------------------------------
void ENetClientNetwork::PollEvents()
{
    if (!m_pClient) return;

    ENetEvent event;
    while (enet_host_service(m_pClient, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            m_IsConnected = true;
            m_Rematching = false;   // handshake completed; curtain may lift
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            m_IsConnected = false;
            m_pServerPeer = nullptr;
            // ENet also reports a connection ATTEMPT that timed out this way,
            // so a failed rematch settles here rather than waiting out the
            // deadline below.
            m_Rematching = false;
            break;

        case ENET_EVENT_TYPE_RECEIVE:
        {
            if (event.packet->dataLength >= 1)
            {
                PacketType type = static_cast<PacketType>(event.packet->data[0]);

                if (type == PacketType::SNAPSHOT &&
                    event.packet->dataLength == 1 + sizeof(Snapshot))
                {
                    Snapshot snap;
                    std::memcpy(&snap, event.packet->data + 1, sizeof(Snapshot));

                    std::lock_guard<std::mutex> lock(m_SnapshotMutex);
                    m_SnapshotQueue.push(snap);
                    m_TotalSnapshotsReceived++;
                }
                else if (type == PacketType::MAP_INFO &&
                         event.packet->dataLength == 1 + sizeof(MapInfo))
                {
                    MapInfo info;
                    std::memcpy(&info, event.packet->data + 1, sizeof(MapInfo));
                    if (m_ExpectedMapChecksum != 0 && info.checksum != m_ExpectedMapChecksum)
                    {
                        printf("[MAP] ERROR: checksum mismatch! server '%s' cksum=%08x local=%08x - disconnecting.\n",
                               info.name, info.checksum, m_ExpectedMapChecksum);
                        enet_peer_disconnect(event.peer, 0);
                    }
                    else
                    {
                        printf("[MAP] verified map '%s' cksum=%08x\n", info.name, info.checksum);
                    }
                }
            }
            enet_packet_destroy(event.packet);
            break;
        }

        default:
            break;
        }
    }

    // Give up on a handshake that never produced either event (server gone
    // dark). Settling here lets the caller's loading curtain lift on a
    // disconnected-but-responsive client instead of hanging on the safety cap.
    if (m_Rematching && ENET_TIME_GREATER_EQUAL(enet_time_get(), m_RematchDeadlineMs))
    {
        if (m_pServerPeer)
        {
            enet_peer_reset(m_pServerPeer);
            m_pServerPeer = nullptr;
        }
        m_Rematching = false;
        printf("[NET] rematch handshake timed out after %ums\n", REMATCH_TIMEOUT_MS);
    }
}

//-----------------------------------------------------------------------------
// BeginRematch - leave this session and start joining a fresh one, non-blocking
//
// Initialize()'s handshake blocks the caller for up to 5 seconds. That is fine
// once at boot, but not mid-session: the frame loop still has to pump the
// window, audio and the loading curtain that is hiding this. So this only KICKS
// OFF the handshake - PollEvents completes it and IsRematchSettled reports when
// the wait is over.
//
// The host is reused rather than torn down: it was created with one outgoing
// peer slot, and freeing the old peer releases that slot for the new connect.
// enet_initialize/deinitialize stay paired with Initialize/Finalize.
//-----------------------------------------------------------------------------
void ENetClientNetwork::BeginRematch()
{
    if (!m_pClient) return;   // never initialized (mock mode keeps the no-op)

    if (m_pServerPeer)
    {
        // disconnect_now sends the notice and frees the peer slot in one call
        // with no wait for an ack - the whole point here. That notice is a
        // single unacknowledged packet, so if it is lost the server keeps a
        // ghost player until its own peer timeout reaps it.
        enet_peer_disconnect_now(m_pServerPeer, 0);
        m_pServerPeer = nullptr;
    }
    m_IsConnected = false;

    // Drop snapshots left from the finished session: they carry the old
    // playerId and the frozen ENDED match, and the game would consume them as
    // if they described the new one.
    {
        std::lock_guard<std::mutex> lock(m_SnapshotMutex);
        std::queue<Snapshot> empty;
        m_SnapshotQueue.swap(empty);
    }

    ENetAddress address;
    enet_address_set_host(&address, m_ServerHost.c_str());
    address.port = m_ServerPort;

    m_pServerPeer = enet_host_connect(m_pClient, &address, 2, 0);
    m_Rematching = (m_pServerPeer != nullptr);
    m_RematchDeadlineMs = enet_time_get() + REMATCH_TIMEOUT_MS;

    m_TotalInputsSent = 0;
    m_TotalSnapshotsReceived = 0;
}

//-----------------------------------------------------------------------------
// SendInputCmd - Serialize and send to server (unreliable)
//-----------------------------------------------------------------------------
void ENetClientNetwork::SendInputCmd(const InputCmd& cmd)
{
    if (!m_pServerPeer || !m_IsConnected) return;

    uint8_t buffer[1 + sizeof(InputCmd)];
    buffer[0] = static_cast<uint8_t>(PacketType::INPUT_CMD);
    std::memcpy(buffer + 1, &cmd, sizeof(InputCmd));

    ENetPacket* packet = enet_packet_create(
        buffer,
        sizeof(buffer),
        ENET_PACKET_FLAG_UNSEQUENCED
    );
    if (!packet) return;

    enet_peer_send(m_pServerPeer, 0, packet);
    m_TotalInputsSent++;
}

//-----------------------------------------------------------------------------
// ReceiveSnapshot - Pop from internal queue
//-----------------------------------------------------------------------------
bool ENetClientNetwork::ReceiveSnapshot(Snapshot& outSnapshot)
{
    std::lock_guard<std::mutex> lock(m_SnapshotMutex);
    if (m_SnapshotQueue.empty()) return false;

    outSnapshot = m_SnapshotQueue.front();
    m_SnapshotQueue.pop();
    return true;
}

size_t ENetClientNetwork::GetSnapshotQueueSize() const
{
    std::lock_guard<std::mutex> lock(m_SnapshotMutex);
    return m_SnapshotQueue.size();
}

//-----------------------------------------------------------------------------
// Network quality stats from ENet peer
//-----------------------------------------------------------------------------
uint32_t ENetClientNetwork::GetRTT() const
{
    if (m_pServerPeer && m_IsConnected)
        return m_pServerPeer->roundTripTime;
    return 0;
}

uint32_t ENetClientNetwork::GetPacketLoss() const
{
    if (m_pServerPeer && m_IsConnected)
        return m_pServerPeer->packetLoss;  // ENet: fixed-point, /65536 for fraction
    return 0;
}

#if defined(_DEBUG)
//-----------------------------------------------------------------------------
// Flood debug mode (Debug builds only).
//-----------------------------------------------------------------------------
void ENetClientNetwork::SetFloodDebug(bool active, int ratePerSec, int mode)
{
    m_FloodActive = active;
    m_FloodRatePerSec = (ratePerSec < 0) ? 0 : ratePerSec;
    m_FloodMode = (mode >= 0 && mode <= 2) ? static_cast<FloodMode>(mode) : FloodMode::Valid;
    if (!active)
    {
        m_FloodSendAccumulator = 0.0;
        m_FloodSendRate = 0.0;
    }
}

void ENetClientNetwork::DriveFloodDebug(double deltaTime)
{
    // Readout window (~1s): actual send rate + this client's own snapshot interval.
    m_FloodStatTimer += deltaTime;
    if (m_FloodStatTimer >= 1.0)
    {
        m_FloodSendRate = m_FloodSentThisWindow / m_FloodStatTimer;
        uint32_t snaps = m_TotalSnapshotsReceived - m_FloodSnapAtWindowStart;
        m_FloodSnapIntervalMs = (snaps > 0) ? (m_FloodStatTimer * 1000.0 / snaps) : 0.0;
        m_FloodStatTimer = 0.0;
        m_FloodSentThisWindow = 0;
        m_FloodSnapAtWindowStart = m_TotalSnapshotsReceived;
    }

    if (!m_FloodActive || !m_pServerPeer || !m_IsConnected) return;

    // Packets owed this frame to approach the target rate, with a per-frame cap so
    // one long frame can't emit a giant burst.
    m_FloodSendAccumulator += deltaTime * m_FloodRatePerSec;
    int toSend = static_cast<int>(m_FloodSendAccumulator);
    m_FloodSendAccumulator -= toSend;
    const int MAX_PER_FRAME = 8192;
    if (toSend > MAX_PER_FRAME) { toSend = MAX_PER_FRAME; m_FloodSendAccumulator = 0.0; }

    for (int i = 0; i < toSend; ++i)
    {
        ENetPacket* packet = nullptr;
        switch (m_FloodMode)
        {
        case FloodMode::Valid:
        {
            // 33-byte valid INPUT_CMD (zeroed = all-finite, passes the server's
            // NaN gate and reaches the tagged-input queue), UNSEQUENCED like the
            // real client — exercises L1 (token bucket) / L3.
            uint8_t buf[1 + sizeof(InputCmd)];
            buf[0] = static_cast<uint8_t>(PacketType::INPUT_CMD);
            InputCmd cmd{};
            std::memcpy(buf + 1, &cmd, sizeof(InputCmd));
            packet = enet_packet_create(buf, sizeof(buf), ENET_PACKET_FLAG_UNSEQUENCED);
            break;
        }
        case FloodMode::Junk:
        {
            // Wrong length + unknown type — exercises the L3 junk-packet path.
            uint8_t buf[5] = { 0xEE, 1, 2, 3, 4 };
            packet = enet_packet_create(buf, sizeof(buf), ENET_PACKET_FLAG_UNSEQUENCED);
            break;
        }
        case FloodMode::Oversized:
        {
            // >1024 bytes, unreliable (sequenced) so ENet fragments it — exercises
            // the server's L2 maximumPacketSize reassembly cap without a reliable
            // retransmit backlog. Use a modest rate for this mode.
            uint8_t buf[1500];
            std::memset(buf, 0xAB, sizeof(buf));
            buf[0] = static_cast<uint8_t>(PacketType::INPUT_CMD);
            packet = enet_packet_create(buf, sizeof(buf), 0);
            break;
        }
        }
        if (packet)
        {
            enet_peer_send(m_pServerPeer, 0, packet);
            m_FloodSentThisWindow++;
        }
    }
}
#endif // _DEBUG

//-----------------------------------------------------------------------------
// No-ops on client side
//-----------------------------------------------------------------------------
bool ENetClientNetwork::ReceiveInputCmd(InputCmd&) { return false; }
size_t ENetClientNetwork::GetInputQueueSize() const { return 0; }
void ENetClientNetwork::SendSnapshot(const Snapshot&) {}
