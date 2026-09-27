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

#include <new>
#include <stdexcept>

#include <sys/resource.h>
#include <sys/time.h>

#include "Exception.h"
#include "GamePacketDispatch.h"
#include "GameServer.h"
#include "KernelContext.h"
#include "Properties.h"
#include "ServerShutdown.h"
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
    struct sigaction action {};
    action.sa_handler = ServerShutdown::request;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGTERM, &action, nullptr) != 0 || sigaction(SIGINT, &action, nullptr) != 0)
        return EXIT_FAILURE;
    ServerShutdown::Deadline shutdownDeadline(std::chrono::seconds(30), "gameserver");
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

    if (argc < 3) {
        // cout << "Usage : gameserver -f config-file" << endl;
        exit(1);
    }

    // Convert the command-line parameters into strings.
    string* Argv;

    Argv = new string[argc];
    for (int i = 0; i < argc; i++)
        Argv[i] = argv[i];

    cout << ">>> COMMAND-LINE PARAMETER READING SUCCESS..." << endl;

    // Read the config file.
    // The executable has to live in $VSHOME/bin and the config file in $VSHOME/conf.
    // The config file can be given on the command line.

    try {
        if (Argv[1] != "-f") {
            throw Error("Usage : gameserver -f config-file -t test-config-file");
        }

        // When the first parameter is -f, the second is the path of the config file.
        Properties* pConfig = new Properties();
        de::kernelContext().setConfig(pConfig);
        pConfig->load(Argv[2]);

        // cout << pConfig->toString() << endl;
    } catch (Error& e) {
        // cout << e.toString() << endl;
    }

    //
    // Create the game server object, initialize it and start it.
    //
    GameServer* pGameServer = NULL;

    try {
        struct rlimit rl;
        rl.rlim_cur = RLIM_INFINITY;
        rl.rlim_max = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rl);

        // Create the game server object.
        pGameServer = new GameServer();

        cout << ">>> GAME SERVER INSTANCE CREATED..." << endl;

        // Initialize the game server object.
        pGameServer->init();

        cout << ">>> GAME SERVER INITIALIZATION SUCCESS..." << endl;

        // Start the game server object.
        if (!ServerShutdown::isRequested())
            pGameServer->start();
    } catch (Throwable& e) {
        // In case the server ends before logging is up.
        ofstream ofile("../log/instant.log", ios::out);
        ofile << e.toString() << endl;
        ofile.close();

        // Print it to standard output as well.
        cout << e.toString() << endl;

        // Stop the game server.
        // The sub-managers have to be stopped from inside it.
        ServerShutdown::fail();
    } catch (...) {
        cout << "unknown exception..." << endl;
        ServerShutdown::fail();
    }
    // Both signal-driven and failed startup paths reach the same teardown.
    ServerShutdown::request();
    bool drained = true;
    try {
        if (pGameServer != NULL)
            pGameServer->stop();
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
    // Legacy singleton destructors do not have a complete dependency order.
    // After every worker has joined, let the OS reclaim the process graph;
    // do not introduce untested singleton destruction on the signal path.
    if (drained)
        cout << ">>> ALL GAME WORKERS STOPPED." << endl;
    cerr.flush();
    std::_Exit(ServerShutdown::failed.load() ? EXIT_FAILURE : EXIT_SUCCESS);
}
