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
#include "ServerShutdown.h"
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
    // SIGTERM/SIGINT only store a lock-free request; the main heartbeat loop
    // and every worker observe it on their next turn.
    struct sigaction action {};
    action.sa_handler = ServerShutdown::request;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGTERM, &action, nullptr) != 0 || sigaction(SIGINT, &action, nullptr) != 0)
        return EXIT_FAILURE;
    // Armed before initialization so a startup that blocks still exits.
    ServerShutdown::Deadline shutdownDeadline(std::chrono::seconds(30), "sharedserver");

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

    //
    // Create the login server object, initialize it and activate it.
    //
    SharedServer* pSharedServer = NULL;

    try {
        struct rlimit rl;
        rl.rlim_cur = RLIM_INFINITY;
        rl.rlim_max = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rl);

        // Create the login server object.
        pSharedServer = new SharedServer();

        // Initialize the login server object.
        pSharedServer->init();

        // Activate the login server object.
        if (!ServerShutdown::isRequested())
            pSharedServer->start();
    } catch (Throwable& e) {
        // In case the server ends before logging is up
        ofstream ofile("../log/instant.log", ios::out);
        ofile << e.toString() << endl;
        ofile.close();

        // It means an exception or error not caught below occurred.
        // Print it on standard output.
        cout << e.toString() << endl;

        // Stop the shared server; every sub-manager has to stop with it.
        ServerShutdown::fail();
    } catch (...) {
        cout << "unknown exception..." << endl;
        ServerShutdown::fail();
    }

    // Both the signal-driven and the failed-startup paths reach the same
    // teardown: request the stop, then join every worker while the managers
    // it uses are still alive.
    ServerShutdown::request();
    bool drained = true;
    try {
        if (pSharedServer != NULL)
            pSharedServer->stop();
    } catch (Throwable& error) {
        drained = false;
        ServerShutdown::fail();
        cerr << "Shutdown failed: " << error.toString() << endl;
    } catch (const std::exception& error) {
        drained = false;
        ServerShutdown::fail();
        cerr << "Shutdown failed: " << error.what() << endl;
    } catch (...) {
        drained = false;
        ServerShutdown::fail();
        cerr << "Shutdown failed: unknown exception" << endl;
    }
    // The legacy singleton graph has no audited destruction order, so let the
    // OS reclaim it once every worker has joined.
    if (drained)
        cout << ">>> ALL SHARED WORKERS STOPPED." << endl;
    cout.flush();
    cerr.flush();
    std::_Exit(ServerShutdown::failed.load() ? EXIT_FAILURE : EXIT_SUCCESS);
}
