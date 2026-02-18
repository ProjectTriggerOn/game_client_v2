//=============================================================================
// enet_client_network.cpp
//
// ENet-based client network implementation.
//=============================================================================

// WinSock2.h must come before Windows.h to avoid winsock.h conflict
#include <WinSock2.h>
#include <enet/enet.h>
#include "enet_client_network.h"
#include "net_packet.h"
#include <cstring>

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
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            m_IsConnected = false;
            m_pServerPeer = nullptr;
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
            }
            enet_packet_destroy(event.packet);
            break;
        }

        default:
            break;
        }
    }
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

//-----------------------------------------------------------------------------
// No-ops on client side
//-----------------------------------------------------------------------------
bool ENetClientNetwork::ReceiveInputCmd(InputCmd&) { return false; }
size_t ENetClientNetwork::GetInputQueueSize() const { return 0; }
void ENetClientNetwork::SendSnapshot(const Snapshot&) {}
