//////////////////////////////////////////////////////////////////////
//
// Filename    : main.cpp
// Written By  : reiot@ewestsoft.com
// Description : Main function for the login server
//
//////////////////////////////////////////////////////////////////////

// include files
#include <stdlib.h>

#include <chrono>
#include <exception>
#include <memory>
#include <new>

#include <sys/resource.h>

#include "Exception.h"
#include "KernelContext.h"
#include "Properties.h"
#include "ServerLifecycle.h"
#include "ServerProcessShutdown.h"
#include "ServerStartup.h"
#include "SharedPacketDispatch.h"
#include "SharedServer.h"
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
    de::ServerProcessShutdown shutdown("sharedserver");
    if (!shutdown.ready())
        return EXIT_FAILURE;

    // Set the out-of-memory handler.
    set_new_handler(memoryError);

    // Bind every packet id the sharedserver receives to its handler before
    // any thread can receive one.
    registerSharedServerPacketHandlers();

    // Keep the completed configuration alive until every worker has stopped.
    // Parsing and loading never publish a partial configuration to the context.
    std::unique_ptr<Properties> pConfig;
    try {
        const auto options = de::parseServerOptions(de::ServerKind::Shared, argc, argv);
        pConfig = de::loadServerConfiguration(options);
        de::kernelContext().setConfig(pConfig.get());
        cout << pConfig->toString() << endl;
    } catch (const Throwable& error) {
        cerr << error.toString() << endl;
        return EXIT_FAILURE;
    }

    SharedServer* pSharedServer = nullptr;
    const de::ServerLifecycleActions lifecycle{
        .initialize =
            [&] {
                struct rlimit rl;
                rl.rlim_cur = RLIM_INFINITY;
                rl.rlim_max = RLIM_INFINITY;
                setrlimit(RLIMIT_CORE, &rl);

                pSharedServer = new SharedServer();
                pSharedServer->init();
            },
        .start = [&] { pSharedServer->start(); },
        .stop =
            [&] {
                if (pSharedServer != nullptr)
                    pSharedServer->stop();
            },
    };
    const auto result = de::runServerLifecycle(lifecycle, cout, cerr);
    // The legacy singleton graph has no audited destruction order, so let the
    // OS reclaim it once every worker has joined.
    if (result.drained)
        cout << ">>> ALL SHARED WORKERS STOPPED." << endl;
    cout.flush();
    cerr.flush();
    std::_Exit(result.exitCode);
}
