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

#include <sys/time.h>

#include "GamePacketDispatch.h"
#include "GameServer.h"
#include "KernelContext.h"
#include "ServerApplication.h"
#include "ServerFatalHandlers.h"
#include "ServerProcessEnvironment.h"
#include "ServerProcessShutdown.h"
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

    de::seedProcessRandomness();
    cout << ">>> RANDOMIZATION INITIALIZATION SUCCESS..." << endl;

    // Bind every packet id the gameserver receives to its handler before any
    // thread can receive a packet.
    registerGameServerPacketHandlers();
    cout << ">>> PACKET DISPATCH TABLE REGISTERED..." << endl;

    GameServer* pGameServer = nullptr;
    const de::ServerLifecycleActions lifecycle{
        .initialize =
            [&] {
                (void)de::raiseCoreDumpLimit();

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
    de::ServerApplication application(de::kernelContext());
    const auto result = application.run(de::ServerKind::Game, argc, argv, lifecycle, cout, cerr);
    if (!result)
        return EXIT_FAILURE;
    // Keep the configuration and legacy server graph alive through process exit.
    std::_Exit(result->exitCode);
}
