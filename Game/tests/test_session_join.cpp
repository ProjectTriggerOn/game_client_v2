//=============================================================================
// test_session_join.cpp
//
// The client's side of the connect/join split. A TriggerOn process opens its
// ENet peer once and keeps it for its whole life, but the person at the
// keyboard is on the title screen for most of that time. Opening a peer must
// therefore say nothing to the server about whether anyone is playing:
//
//   Initialize()        create the host. No peer, no server contact.
//   BeginJoinSession()  connect, verify the map, send JOIN_REQUEST, and settle
//                       once the server answers with a snapshot.
//   LeaveSession()      drop out of the room again (title screen, match over).
//
// Needs a real game_server to talk to - this is the half of the protocol the
// server suite (tools/run_sessiontest.sh) cannot check by itself.
//
//   game_server --port=7905 --map=shipment.map        (on the server host)
//
// Build (VS developer prompt, from the repository root):
//   cl /nologo /std:c++17 /EHsc /W4 /I . /I Network /I ThirdParty\enet\include ^
//      Game\tests\test_session_join.cpp Network\enet_client_network.cpp ^
//      ThirdParty\enet\lib\enet.lib ws2_32.lib winmm.lib /Fe:_test_session_join.exe
//   _test_session_join.exe --host 192.168.5.24 --port 7905
//=============================================================================

#include "enet_client_network.h"
#include "net_packet.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <string>

using Clock = std::chrono::steady_clock;

static int g_Failures = 0;

static bool Check(const char* what, bool ok)
{
    std::printf("   %-4s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) g_Failures++;
    return ok;
}

//-----------------------------------------------------------------------------
// Pump the network for `seconds`, as the frame loop does, optionally stopping
// early once `done` reports true. Returns the seconds actually spent.
//-----------------------------------------------------------------------------
template <typename F>
static double Pump(ENetClientNetwork& net, double seconds, F done)
{
    const auto t0 = Clock::now();
    for (;;)
    {
        const double el = std::chrono::duration<double>(Clock::now() - t0).count();
        if (el >= seconds || done()) return el;
        net.PollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
}

static void PumpFor(ENetClientNetwork& net, double seconds)
{
    Pump(net, seconds, [] { return false; });
}

// Drain and count, the way Game_Update does once per frame.
static size_t DrainSnapshots(ENetClientNetwork& net)
{
    Snapshot s;
    size_t n = 0;
    while (net.ReceiveSnapshot(s)) n++;
    return n;
}

//=============================================================================
int main(int argc, char** argv)
{
    std::string host = "127.0.0.1";
    uint16_t port = 7905;
    for (int i = 1; i < argc; i++)
    {
        if (!std::strcmp(argv[i], "--host") && i + 1 < argc) host = argv[++i];
        else if (!std::strcmp(argv[i], "--port") && i + 1 < argc)
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
    }

    std::printf("=== test_session_join (server %s:%u) ===\n", host.c_str(), port);

    ENetClientNetwork net;
    net.SetServerAddress(host.c_str(), port);

    //-------------------------------------------------------------------------
    std::printf("\n-- [1/5] Initialize() opens no session\n");
    // Booting the process must not put anyone in a match. This used to connect
    // right here, which is how two clients left on the title screen started -
    // and finished - a 60-second round nobody was playing.
    const auto tBoot = Clock::now();
    net.Initialize();
    const double bootSecs = std::chrono::duration<double>(Clock::now() - tBoot).count();

    Check("Initialize() is not connected to a server", !net.IsConnected());
    Check("Initialize() does not block on a handshake (< 1s)", bootSecs < 1.0);
    std::printf("        Initialize() took %.0f ms\n", bootSecs * 1000.0);

    PumpFor(net, 1.0);
    Check("still no session after a second of pumping", !net.IsConnected());
    Check("and no snapshots arrived", net.GetSnapshotQueueSize() == 0);

    //-------------------------------------------------------------------------
    std::printf("\n-- [2/5] BeginJoinSession() joins the match room\n");
    net.BeginJoinSession();
    Check("the join does not block the caller", !net.IsJoinSettled());

    const double settleSecs = Pump(net, 8.0, [&] { return net.IsJoinSettled(); });
    Check("the join settles", net.IsJoinSettled());
    Check("...connected", net.IsConnected());
    std::printf("        settled after %.0f ms\n", settleSecs * 1000.0);

    // Settling means the server has ACCEPTED us, not merely that a UDP peer
    // exists: it answers a join with snapshots, and the first one is what the
    // caller's loading curtain is really waiting for.
    Check("a snapshot was waiting the moment the join settled",
          net.GetSnapshotQueueSize() > 0);

    //-------------------------------------------------------------------------
    std::printf("\n-- [3/5] a joined client receives the world\n");
    DrainSnapshots(net);
    PumpFor(net, 1.0);
    const size_t perSecond = DrainSnapshots(net);
    // 32Hz, minus whatever a loopback-or-LAN hop loses. Anything above zero
    // proves the stream; the band catches a server stuck off-tick.
    Check("snapshots stream at roughly the 32Hz tick rate",
          perSecond >= 20 && perSecond <= 40);
    std::printf("        %zu snapshots in 1.0s\n", perSecond);

    //-------------------------------------------------------------------------
    std::printf("\n-- [4/5] the snapshot queue is bounded\n");
    // Nothing drains while the client is not in the game scene. The queue used
    // to grow without limit - ~2400 snapshots (1.8 MB) per 75 seconds on the
    // title screen - and the whole backlog was then replayed in a single frame
    // the moment the player pressed PLAY, walking them through a match that had
    // already ended.
    PumpFor(net, 3.0);
    const size_t queued = net.GetSnapshotQueueSize();
    Check("an undrained queue stays bounded",
          queued <= ENetClientNetwork::MAX_QUEUED_SNAPSHOTS);
    std::printf("        %zu queued after 3s undrained (cap %zu)\n",
                queued, ENetClientNetwork::MAX_QUEUED_SNAPSHOTS);

    // Dropping the OLDEST is what makes the cap safe: a snapshot is a complete
    // world state, so the newest is the only one that matters. Keeping the
    // oldest would pin the client in the past.
    DrainSnapshots(net);
    PumpFor(net, 0.5);
    Snapshot newest{};
    uint32_t firstTick = 0, lastTick = 0;
    bool got = false;
    while (net.ReceiveSnapshot(newest))
    {
        if (!got) { firstTick = newest.tickId; got = true; }
        lastTick = newest.tickId;
    }
    Check("the queue hands back snapshots in tick order", got && lastTick >= firstTick);

    //-------------------------------------------------------------------------
    std::printf("\n-- [5/5] LeaveSession() gives the slot back\n");
    net.LeaveSession();
    PumpFor(net, 1.0);
    Check("no longer connected", !net.IsConnected());
    Check("queued snapshots from the old session are dropped",
          net.GetSnapshotQueueSize() == 0);
    PumpFor(net, 1.0);
    Check("and no new ones arrive", net.GetSnapshotQueueSize() == 0);

    net.Finalize();

    std::printf("\n=== test_session_join: %s (%d failure%s) ===\n",
                g_Failures == 0 ? "PASS" : "FAIL",
                g_Failures, g_Failures == 1 ? "" : "s");
    return g_Failures == 0 ? 0 : 1;
}
