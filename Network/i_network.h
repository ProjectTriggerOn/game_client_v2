#pragma once
//=============================================================================
// i_network.h
//
// Abstract network interface for Server-Authoritative architecture.
// Allows swapping between MockNetwork (local testing) and ENet (real network).
//=============================================================================

#include "net_common.h"
#include <cstdint>

class INetwork
{
public:
    virtual ~INetwork() = default;

    virtual void Initialize() = 0;
    virtual void Finalize() = 0;

    //-------------------------------------------------------------------------
    // Client -> Server (Upstream)
    //-------------------------------------------------------------------------
    virtual void SendInputCmd(const InputCmd& cmd) = 0;
    virtual bool ReceiveInputCmd(InputCmd& outCmd) = 0;
    virtual size_t GetInputQueueSize() const = 0;

    //-------------------------------------------------------------------------
    // Server -> Client (Downstream)
    //-------------------------------------------------------------------------
    virtual void SendSnapshot(const Snapshot& snapshot) = 0;
    virtual bool ReceiveSnapshot(Snapshot& outSnapshot) = 0;
    virtual size_t GetSnapshotQueueSize() const = 0;

    //-------------------------------------------------------------------------
    // Debug / Statistics
    //-------------------------------------------------------------------------
    virtual uint32_t GetTotalInputsSent() const = 0;
    virtual uint32_t GetTotalSnapshotsSent() const = 0;

    // Network quality (ENet only; mock returns defaults)
    virtual uint32_t GetRTT() const { return 0; }
    virtual uint32_t GetPacketLoss() const { return 0; }
    virtual bool IsConnected() const { return true; }

    //-------------------------------------------------------------------------
    // Match-room membership. A finished match leaves the client on the result
    // screen, and being ON that screen must not count as occupying the server's
    // room - otherwise one player pressing NEXT MATCH reaches the minimum
    // player count on their own and drags everyone still reading the scoreboard
    // into the next round. So the client LEAVES when the match ends, and
    // rejoining is what the button does.
    //
    // Both are non-blocking: the caller hides the gap behind the loading curtain
    // and polls IsRematchSettled to know when to lift it. The mock network has
    // no session to leave or rejoin, so it no-ops and settles immediately - the
    // client-side reset in Game_Initialize is the whole rematch there.
    //-------------------------------------------------------------------------
    virtual void LeaveSession() {}
    virtual void BeginRematch() {}
    virtual bool IsRematchSettled() const { return true; }
};
