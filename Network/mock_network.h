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

#include "i_network.h"
#include <queue>
#include <mutex>

class MockNetwork : public INetwork
{
public:
    MockNetwork() = default;
    ~MockNetwork() override = default;

    void Initialize() override;
    void Finalize() override;

    //-------------------------------------------------------------------------
    // Client -> Server (Upstream)
    //-------------------------------------------------------------------------
    void SendInputCmd(const InputCmd& cmd) override;
    bool ReceiveInputCmd(InputCmd& outCmd) override;
    size_t GetInputQueueSize() const override;

    //-------------------------------------------------------------------------
    // Server -> Client (Downstream)
    //-------------------------------------------------------------------------
    void SendSnapshot(const Snapshot& snapshot) override;
    bool ReceiveSnapshot(Snapshot& outSnapshot) override;
    size_t GetSnapshotQueueSize() const override;

    //-------------------------------------------------------------------------
    // Debug / Statistics
    //-------------------------------------------------------------------------
    uint32_t GetTotalInputsSent() const override { return m_TotalInputsSent; }
    uint32_t GetTotalSnapshotsSent() const override { return m_TotalSnapshotsSent; }

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
