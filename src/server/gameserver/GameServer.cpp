//////////////////////////////////////////////////////////////////////////////
// Filename    : GameServer.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GameServer.h"

#include <unistd.h>

#include "Assert.h"
#include "ClientManager.h"
#include "DatabaseManager.h"
#include "GameContext.h"
#include "GameServerInfoManager.h"
#include "GuildUnion.h"
#include "KernelContext.h"
#include "LoginServerManager.h"
#include "ObjectManager.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "Properties.h"
#include "ServerContext.h"
#include "ServerProcessEnvironment.h"
#include "ServerStartSequence.h"
#include "ServerWorkerShutdown.h"
#include "SharedServerManager.h"
#include "SystemAPI.h"
#include "ThreadManager.h"
#include "exchange/ExchangeService.h"
#include "mofus/Mofus.h"
#ifdef __MOFUS__
#include "mofus/MPacketManager.h"
#include "mofus/MPlayerManager.h"
#endif

#include "GDRLairManager.h"
#include "SMSServiceThread.h"


////////////////////////////////////////////////////////////////////////////////
// constructor
////////////////////////////////////////////////////////////////////////////////

GameServer::GameServer()

{
    __BEGIN_TRY

    try {
        // The configuration is loaded before the server object exists, so it
        // is registered first; every manager below registers as it is created.
        de::gameContext().setConfig(&de::kernelContext().config());

        // create database manager
        m_pDatabaseManager = new DatabaseManager();
        de::serverContext().setDatabaseManager(m_pDatabaseManager);

        // create object manager
        m_pObjectManager = new ObjectManager();

        // create packet factory manager , packet validator
        m_pPacketFactoryManager = new PacketFactoryManager();
        de::kernelContext().setPacketFactoryManager(m_pPacketFactoryManager);
        m_pPacketValidator = new PacketValidator();
        de::kernelContext().setPacketValidator(m_pPacketValidator);

        // create thread manager
        m_pThreadManager = new ThreadManager();

        // create login server manager
        m_pLoginServerManager = new LoginServerManager();
        de::gameContext().setLoginServerManager(m_pLoginServerManager);

        // create shared server manager
        m_pSharedServerManager = new SharedServerManager();
        de::gameContext().setSharedServerManager(m_pSharedServerManager);

#ifdef __MOFUS__
        m_pMPlayerManager = new MPlayerManager();
        de::gameContext().setMPlayerManager(m_pMPlayerManager);
        m_pMPacketManager = new MPacketManager();
        de::gameContext().setMPacketManager(m_pMPacketManager);
#endif

        // create client manager
        m_pClientManager = new ClientManager();
        de::gameContext().setClientManager(m_pClientManager);

        // create the game-server table
        m_pGameServerInfoManager = new GameServerInfoManager();
        de::serverContext().setGameServerInfoManager(m_pGameServerInfoManager);

    } catch (Throwable& t) {
        // cout << t.toString() << endl;
        throw;
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// destructor
// Must check for the game server terminating without going through stop().
//////////////////////////////////////////////////////////////////////////////

GameServer::~GameServer()

{
    __BEGIN_TRY

    stop();
    // Zone workers must stop while their zone and database dependencies are
    // still alive.
    SAFE_DELETE(m_pThreadManager);
    SAFE_DELETE(m_pClientManager);
    SAFE_DELETE(m_pObjectManager);
    SAFE_DELETE(m_pPacketValidator);
    SAFE_DELETE(m_pPacketFactoryManager);
    SAFE_DELETE(m_pLoginServerManager);
    SAFE_DELETE(m_pSharedServerManager);
#ifdef __MOFUS__
    SAFE_DELETE(m_pMPlayerManager);
    SAFE_DELETE(m_pMPacketManager);
#endif
    SAFE_DELETE(m_pGameServerInfoManager);
    SAFE_DELETE(m_pDatabaseManager);

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// initialize game server
//////////////////////////////////////////////////////////////////////////////

void GameServer::init()

{
    __BEGIN_TRY

    de::initializeGameProcess();
    cout << "GameServer::init() : System Initialization Success..." << endl;

    // Set gCurrentTime.
    setCurrentTime();

    // Initialize the database manager.
    m_pDatabaseManager->init();
    cout << "GameServer::init() : DatabaseManager Initialization Success..." << endl;

    // The Exchange's point ledger is reached from the game connection by the
    // account schema's name; a deployment where that fails runs without
    // Exchange purchases, and this is where it is told so.
    ExchangeService::openPointLedger();

    // Initialize the object manager through the database manager.
    m_pObjectManager->init();
    m_pObjectManager->load();
    cout << "GameServer::init() : ObjectManager Initialization Success..." << endl;

    // Initialize the thread manager on top of the object manager.
    // (In particular ZoneThreadPool requires ZoneGroupManager to be initialized first.)
    m_pThreadManager->init();
    cout << "GameServer::init() : ThreadManager Initialization Success..." << endl;

    // Initialize the packet factory manager and packet validator before the client manager.
    m_pPacketFactoryManager->init();
    cout << "GameServer::init() : PacketFactoryManager Initialization Success..." << endl;

    m_pPacketValidator->init();
    cout << "GameServer::init() : PacketValidator Initialization Success..." << endl;

    // Now prepare the inter-server communication.
    m_pLoginServerManager->init();
    cout << "GameServer::init() : LoginServerManager Initialization Success..." << endl;

    // Prepare the communication with the shared server.
    m_pSharedServerManager->init();
    cout << "GameServer::init() : SharedServerManager Initialization Success..." << endl;

#ifdef __MOFUS__
    m_pMPacketManager->init();
    cout << "GameServer::init() : MPacketManager Initialization Success..." << endl;

    m_pMPlayerManager->init();
    cout << "GameServer::init() : MPlayerManager Initialization Success..." << endl;
#endif

    m_pGameServerInfoManager->init();
    cout << "GameServer::init() : GameServerInfoManager Initialization Success..." << endl;

    // A union the startup load dissolved is still held by the other game
    // servers of the world; they can be told now that the link is up.
    GuildUnionManager::Instance().sendOwedRefresh();

    // Once everything else is ready, initialize the client manager to
    // prepare for networking.
    m_pClientManager->init();
    cout << "GameServer::init() : ClientManager Initialization Success..." << endl;

    // When initialization is done, stop the console output and go to the background.
    // goBackground();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// start game server
//////////////////////////////////////////////////////////////////////////////

void GameServer::start()

{
    __BEGIN_TRY

    const de::ServerStartAction backgroundStarts[] = {
        [this] {
            cout << ">>> STARTING THREAD MANAGER..." << endl;
            m_pThreadManager->start();
        },
        [this] {
            cout << ">>> STARTING LOGIN SERVER MANAGER..." << endl;
            m_pLoginServerManager->start();
        },
        [this] {
            cout << ">>> STARTING SHARED SERVER MANAGER..." << endl;
            m_pSharedServerManager->start();
        },
#ifdef __MOFUS__
        [this] {
            m_pMPlayerManager->start();
            cout << ">>> STARTING MOFUS PLAYER MANAGER..." << endl;
        },
#endif
        [] { GDRLairManager::Instance().init(); },
        [] { GDRLairManager::Instance().start(); },
    };
    de::runServerStartSequence(backgroundStarts, [this] {
        // The blocking client loop starts only after every background step.
        cout << ">>> ALL INITIALIZATIONS ARE COMPLETED SUCCESSFULLY." << endl;
        cout << ">>> STARTING ClientManager->start() INFINITE LOOP..." << endl;
        try {
            m_pClientManager->start();
        } catch (Throwable& t) {
            filelog("GameServerError.txt", "%s", t.toString().c_str());
            throw;
        }
    });

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// stop game server
//
// Mind the stop order. The managers with the widest effect must be
// stopped first. Stopping them in the reverse order can produce null
// pointer problems.
//
// So the thread-related managers must be destroyed first.
//////////////////////////////////////////////////////////////////////////////

void GameServer::stop()

{
    if (m_Stopped)
        return;
    __BEGIN_TRY

    //
    // stop client manager
    //
    // Destroy the client manager first so that no new connections are
    // accepted any more.
    //
    ServerShutdown::request();
    m_pClientManager->stop();
    // Auxiliary workers and the zone pool drain while their shared dependencies
    // remain alive. Ask every auxiliary worker to stop before draining zones.
    const de::ServerWorker workers[] = {
        {*m_pLoginServerManager},
        {*m_pSharedServerManager},
        {GDRLairManager::Instance()},
#ifdef __MOFUS__
        {*m_pMPlayerManager},
#endif
    };
    de::stopServerWorkers(workers, cerr, [&] { m_pThreadManager->stop(); });
    m_Stopped = true;

    // stop object manager
    //
    // Now that every user connection is closed, save the remaining zones and
    // the various game settings to the database.
    //
    // m_pObjectManager->save();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Call this function once the server is stable enough that console
// output is no longer needed.
//////////////////////////////////////////////////////////////////////////////

void GameServer::goBackground()

{
    __BEGIN_TRY

    int forkres = SystemAPI::fork_ex();

    if (forkres == 0) {
        // case of child process
        close(0);
        close(1);
        close(2);
    } else {
        // case of parent process
        exit(0);
    }

    __END_CATCH
}
