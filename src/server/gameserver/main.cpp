//////////////////////////////////////////////////////////////////////
//
// Filename    : main.cpp
// Written By  : reiot@ewestsoft.com
// Description : main function for the game server
//
//////////////////////////////////////////////////////////////////////

// include files
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory>
#include <new>
#include <stdexcept>

#include <sys/resource.h>
#include <sys/time.h>

#include "Exception.h"
#include "GamePacketDispatch.h"
#include "GameServer.h"
#include "KernelContext.h"
#include "Properties.h"
#include "ServerLifecycle.h"
#include "ServerProcessShutdown.h"
#include "ServerStartup.h"
#include "StringStream.h"
#include "Types.h"

void handleMemoryError() {
    cerr << "==============================================================================" << endl;
    cerr << "CRITICAL ERROR! NOT ENOUGH MEMORY!" << endl;
    cerr << "==============================================================================" << endl;
    filelog("CriticalError.log", "CRITICAL ERROR! NOT ENOUGH MEMORY!");
    abort();
}

void handleUnhandledException() {
    cerr << "==============================================================================" << endl;
    cerr << "UNHANDLED EXCEPTION OCCURED" << endl;
    cerr << "==============================================================================" << endl;
    filelog("CriticalError.log", "UNHANDLED EXCEPTION OCCURED");
    abort();
}

void testMaxMemory() {
    long mem = 10 * 1024 * 1024; // 10M

    char str[80];

    for (int i = 1; i < 2048; i++) {
        char* p = new char[mem];

        snprintf(str, sizeof(str), "%p = %04d0 M", (void*)p, i);

        cout << str << endl;
    }
}

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

    // Install the various handlers.
    std::set_new_handler(handleMemoryError);
    std::set_terminate(handleUnhandledException);

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
