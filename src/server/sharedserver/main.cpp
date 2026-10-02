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

#include "KernelContext.h"
#include "ServerApplication.h"
#include "ServerFatalHandlers.h"
#include "ServerProcessEnvironment.h"
#include "ServerProcessShutdown.h"
#include "SharedPacketDispatch.h"
#include "SharedServer.h"
#include "StringStream.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////
//
// main()
//
//////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]) {
    de::ServerProcessShutdown shutdown("sharedserver");
    if (!shutdown.ready())
        return EXIT_FAILURE;

    de::ServerFatalHandlers fatalHandlers(de::ServerKind::Shared);

    // Bind every packet id the sharedserver receives to its handler before
    // any thread can receive one.
    registerSharedServerPacketHandlers();

    SharedServer* pSharedServer = nullptr;
    const de::ServerLifecycleActions lifecycle{
        .initialize =
            [&] {
                (void)de::raiseCoreDumpLimit();

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
    de::ServerApplication application(de::kernelContext());
    const auto result = application.run(de::ServerKind::Shared, argc, argv, lifecycle, cout, cerr);
    if (!result)
        return EXIT_FAILURE;
    // Keep the configuration and legacy server graph alive through process exit.
    std::_Exit(result->exitCode);
}
