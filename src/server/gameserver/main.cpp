//////////////////////////////////////////////////////////////////////
//
// Filename    : main.cpp
// Written By  : reiot@ewestsoft.com
// Description : main function for the game server
//
//////////////////////////////////////////////////////////////////////

// include files
#include <stdlib.h>
#include <unistd.h>

#include <memory>

#include <sys/resource.h>
#include <sys/time.h>

#include "Exception.h"
#include "GamePacketDispatch.h"
#include "GameServer.h"
#include "KernelContext.h"
#include "Properties.h"
#include "ServerFatalHandlers.h"
#include "ServerLifecycle.h"
#include "ServerProcessShutdown.h"
#include "ServerStartup.h"
#include "StringStream.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////
//
// main()
//
//////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]) {
    de::ServerProcessShutdown shutdown("gameserver");
    if (!shutdown.ready())
        return EXIT_FAILURE;
    cout << ">>> STARTING GAME SERVER..." << endl;

    filelog("serverStart.log", "GameServer Start");

    de::ServerFatalHandlers fatalHandlers(de::ServerKind::Game);

    // Find a suitable place for this.
    srand(time(0));
    cout << ">>> RANDOMIZATION INITIALIZATION SUCCESS..." << endl;

    // Bind every packet id the gameserver receives to its handler before any
    // thread can receive a packet.
    registerGameServerPacketHandlers();
    cout << ">>> PACKET DISPATCH TABLE REGISTERED..." << endl;

    // Keep the completed configuration alive until every worker has stopped.
    // Parsing and loading never publish a partial configuration to the context.
    std::unique_ptr<Properties> pConfig;
    try {
        const auto options = de::parseServerOptions(de::ServerKind::Game, argc, argv);
        cout << ">>> COMMAND-LINE PARAMETER READING SUCCESS..." << endl;
        pConfig = de::loadServerConfiguration(options);
        de::kernelContext().setConfig(pConfig.get());
    } catch (const Throwable& error) {
        cerr << error.toString() << endl;
        return EXIT_FAILURE;
    }

    GameServer* pGameServer = nullptr;
    const de::ServerLifecycleActions lifecycle{
        .initialize =
            [&] {
                struct rlimit rl;
                rl.rlim_cur = RLIM_INFINITY;
                rl.rlim_max = RLIM_INFINITY;
                setrlimit(RLIMIT_CORE, &rl);

                pGameServer = new GameServer();
                cout << ">>> GAME SERVER INSTANCE CREATED..." << endl;
                pGameServer->init();
                cout << ">>> GAME SERVER INITIALIZATION SUCCESS..." << endl;
            },
        .start = [&] { pGameServer->start(); },
        .stop =
            [&] {
                if (pGameServer != nullptr)
                    pGameServer->stop();
            },
    };
    const auto result = de::runServerLifecycle(lifecycle, cout, cerr);
    // Legacy singleton destructors do not have a complete dependency order.
    // After every worker has joined, let the OS reclaim the process graph;
    // do not introduce untested singleton destruction on the signal path.
    if (result.drained)
        cout << ">>> ALL GAME WORKERS STOPPED." << endl;
    cerr.flush();
    std::_Exit(result.exitCode);
}
