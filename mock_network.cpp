//=============================================================================
// mock_network.cpp
//
// Mock network implementation using std::queue.
// Thread-safe for future async usage.
//=============================================================================

#include "mock_network.h"

void MockNetwork::Initialize()
{
    // Clear any existing data
    std::lock_guard<std::mutex> upLock(m_UpstreamMutex);
    std::lock_guard<std::mutex> downLock(m_DownstreamMutex);
    
    while (!m_UpstreamQueue.empty()) m_UpstreamQueue.pop();
    while (!m_DownstreamQueue.empty()) m_DownstreamQueue.pop();
    
    m_TotalInputsSent = 0;
    m_TotalSnapshotsSent = 0;
}

void MockNetwork::Finalize()
{
    // Clear queues
    std::lock_guard<std::mutex> upLock(m_UpstreamMutex);
    std::lock_guard<std::mutex> downLock(m_DownstreamMutex);
    
    while (!m_UpstreamQueue.empty()) m_UpstreamQueue.pop();
    while (!m_DownstreamQueue.empty()) m_DownstreamQueue.pop();
}

//-----------------------------------------------------------------------------
// Client -> Server (Upstream)
//-----------------------------------------------------------------------------

void MockNetwork::SendInputCmd(const InputCmd& cmd)
{
    std::lock_guard<std::mutex> lock(m_UpstreamMutex);
    m_UpstreamQueue.push(cmd);
    m_TotalInputsSent++;
}

bool MockNetwork::ReceiveInputCmd(InputCmd& outCmd)
{
    std::lock_guard<std::mutex> lock(m_UpstreamMutex);
    if (m_UpstreamQueue.empty())
    {
        return false;
    }
    outCmd = m_UpstreamQueue.front();
    m_UpstreamQueue.pop();
    return true;
}

size_t MockNetwork::GetInputQueueSize() const
{
    std::lock_guard<std::mutex> lock(m_UpstreamMutex);
    return m_UpstreamQueue.size();
}

//-----------------------------------------------------------------------------
// Server -> Client (Downstream)
//-----------------------------------------------------------------------------

void MockNetwork::SendSnapshot(const Snapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(m_DownstreamMutex);
    m_DownstreamQueue.push(snapshot);
    m_TotalSnapshotsSent++;
}

bool MockNetwork::ReceiveSnapshot(Snapshot& outSnapshot)
{
    std::lock_guard<std::mutex> lock(m_DownstreamMutex);
    if (m_DownstreamQueue.empty())
    {
        return false;
    }
    outSnapshot = m_DownstreamQueue.front();
    m_DownstreamQueue.pop();
    return true;
}

size_t MockNetwork::GetSnapshotQueueSize() const
{
    std::lock_guard<std::mutex> lock(m_DownstreamMutex);
    return m_DownstreamQueue.size();
}
