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

#include <sys/time.h>

#include "KernelContext.h"
#include "LoginPacketDispatch.h"
#include "LoginServer.h"
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
    de::ServerProcessShutdown shutdown("loginserver");
    if (!shutdown.ready())
        return EXIT_FAILURE;

    de::ServerFatalHandlers fatalHandlers(de::ServerKind::Login);

    // Bind every packet id the loginserver receives to its handler before
    // any thread can receive one.
    registerLoginServerPacketHandlers();

    LoginServer* pLoginServer = nullptr;
    const de::ServerLifecycleActions lifecycle{
        .initialize =
            [&] {
                (void)de::raiseCoreDumpLimit();

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
    de::ServerApplication application(de::kernelContext());
    const auto result = application.run(de::ServerKind::Login, argc, argv, lifecycle, cout, cerr);
    if (!result)
        return EXIT_FAILURE;
    // Keep the configuration and legacy server graph alive through process exit.
    std::_Exit(result->exitCode);
}
