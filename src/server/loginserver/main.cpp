//////////////////////////////////////////////////////////////////////
//
// Filename    : main.cpp
// Written By  : reiot@ewestsoft.com
// Description : Main function for the login server
//
//////////////////////////////////////////////////////////////////////

// include files
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <chrono>
#include <exception>
#include <memory>
#include <new>

#include <sys/resource.h>
#include <sys/time.h>

#include "Exception.h"
#include "KernelContext.h"
#include "LoginPacketDispatch.h"
#include "LoginServer.h"
#include "Properties.h"
#include "ServerLifecycle.h"
#include "ServerProcessShutdown.h"
#include "ServerStartup.h"
#include "StringStream.h"
#include "Types.h"

void memoryError() {
    cout << "CRITICAL ERROR! NOT ENOUGH MEMORY!" << endl;
    exit(0);
}

//////////////////////////////////////////////////////////////////////
//
// main()
//
//////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]) {
    de::ServerProcessShutdown shutdown("loginserver");
    if (!shutdown.ready())
        return EXIT_FAILURE;

    // Set the out-of-memory handler.
    set_new_handler(memoryError);

    // Bind every packet id the loginserver receives to its handler before
    // any thread can receive one.
    registerLoginServerPacketHandlers();

    // Keep the completed configuration alive until every worker has stopped.
    // Parsing and loading never publish a partial configuration to the context.
    std::unique_ptr<Properties> pConfig;
    try {
        const auto options = de::parseServerOptions(de::ServerKind::Login, argc, argv);
        pConfig = de::loadServerConfiguration(options);
        de::kernelContext().setConfig(pConfig.get());
        cout << pConfig->toString() << endl;
        if (options.loginIDOffset) {
            cout << "LoginServerPort : " << pConfig->getProperty("LoginServerPort") << endl;
            cout << "LoginServerUDPPort : " << pConfig->getProperty("LoginServerUDPPort") << endl;
            cout << "LoginServerID : " << pConfig->getProperty("LoginServerID") << endl;
        }
    } catch (const Throwable& error) {
        cerr << error.toString() << endl;
        return EXIT_FAILURE;
    }

    LoginServer* pLoginServer = nullptr;
    const de::ServerLifecycleActions lifecycle{
        .initialize =
            [&] {
                struct rlimit rl;
                rl.rlim_cur = RLIM_INFINITY;
                rl.rlim_max = RLIM_INFINITY;
                setrlimit(RLIMIT_CORE, &rl);

                pLoginServer = new LoginServer();
                pLoginServer->init();
            },
        .start = [&] { pLoginServer->start(); },
        .stop =
            [&] {
                if (pLoginServer != nullptr)
                    pLoginServer->stop();
            },
    };
    const auto result = de::runServerLifecycle(lifecycle, cout, cerr);
    // The legacy singleton graph has no audited destruction order, so let the
    // OS reclaim it once every worker has joined.
    if (result.drained)
        cout << ">>> ALL LOGIN WORKERS STOPPED." << endl;
    cout.flush();
    cerr.flush();
    std::_Exit(result.exitCode);
}
