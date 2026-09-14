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
    // Match-room membership - separate from the transport connection on
    // purpose. A client holds its peer for the life of the process, but a
    // person on the title screen or the result screen is not playing, and must
    // not occupy a place in the server's room: if they did, two clients left on
    // a menu would reach the minimum player count between them and run a match
    // with nobody in the world, and one player pressing NEXT MATCH would drag
    // everyone still reading the scoreboard into the next round.
    //
    // So the client joins when the player enters the game (PLAY / NEXT MATCH)
    // and leaves when they go back to a menu or the match ends.
    //
    // Both are non-blocking: the caller hides the gap behind the loading curtain
    // and polls IsJoinSettled to know when to lift it. The mock network has no
    // room to join or leave, so it no-ops and settles immediately - the
    // client-side reset in Game_Initialize is the whole rematch there.
    //-------------------------------------------------------------------------
    virtual void LeaveSession() {}
    virtual void BeginJoinSession() {}
    virtual bool IsJoinSettled() const { return true; }
};
