#pragma once
//=============================================================================
// mock_network.h
// 
// Mock network layer using std::queue for local testing.
// Simulates network communication between client and server.
//
// This abstraction allows future replacement with real network (ENet, etc.)
// without changing game logic.
//=============================================================================

#include "net_common.h"
#include <queue>
#include <mutex>

class MockNetwork
{
public:
    MockNetwork() = default;
    ~MockNetwork() = default;

    void Initialize();
    void Finalize();

    //-------------------------------------------------------------------------
    // Client -> Server (Upstream)
    //-------------------------------------------------------------------------
    void SendInputCmd(const InputCmd& cmd);
    bool ReceiveInputCmd(InputCmd& outCmd);
    size_t GetInputQueueSize() const;

    //-------------------------------------------------------------------------
    // Server -> Client (Downstream)
    //-------------------------------------------------------------------------
    void SendSnapshot(const Snapshot& snapshot);
    bool ReceiveSnapshot(Snapshot& outSnapshot);
    size_t GetSnapshotQueueSize() const;

    //-------------------------------------------------------------------------
    // Debug / Statistics
    //-------------------------------------------------------------------------
    uint32_t GetTotalInputsSent() const { return m_TotalInputsSent; }
    uint32_t GetTotalSnapshotsSent() const { return m_TotalSnapshotsSent; }

private:
    // Upstream: Client -> Server
    std::queue<InputCmd> m_UpstreamQueue;
    mutable std::mutex m_UpstreamMutex;

    // Downstream: Server -> Client
    std::queue<Snapshot> m_DownstreamQueue;
    mutable std::mutex m_DownstreamMutex;

    // Statistics
    uint32_t m_TotalInputsSent = 0;
    uint32_t m_TotalSnapshotsSent = 0;
};
