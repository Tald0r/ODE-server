//////////////////////////////////////////////////////////////////////
//
// Filename    : LoginServer.cpp
// Written By  : reiot@ewestsoft.com
// Description : Main class for the login server
//
//////////////////////////////////////////////////////////////////////

// include files
#include "LoginServer.h"

#include "Assert.h"
#include "ClientManager.h"
#include "GameServerGroupInfoManager.h"
#include "GameServerInfoManager.h"
#include "GameServerManager.h"
#include "GameWorldInfoManager.h"
#include "ItemDestroyer.h"
#include "KernelContext.h"
#include "LoginContext.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "ServerContext.h"
#include "ServerShutdown.h"
#include "ServerWorkerShutdown.h"
#include "UserInfoManager.h"
#include "ZoneGroupInfoManager.h"
#include "ZoneInfoManager.h"
#include "database/DatabaseManager.h"

//////////////////////////////////////////////////////////////////////
//
// constructor
//
// The system manager's constructor creates the sub-manager objects.
//
//////////////////////////////////////////////////////////////////////
LoginServer::LoginServer() {
    __BEGIN_TRY

    // create database manager
    m_pDatabaseManager = new DatabaseManager();
    de::serverContext().setDatabaseManager(m_pDatabaseManager);

    // create some info managers
    m_pGameServerInfoManager = new GameServerInfoManager();
    de::serverContext().setGameServerInfoManager(m_pGameServerInfoManager);
    m_pGameServerGroupInfoManager = new GameServerGroupInfoManager();
    de::loginContext().setGameServerGroupInfoManager(m_pGameServerGroupInfoManager);

    m_pZoneInfoManager = new ZoneInfoManager();
    de::loginContext().setZoneInfoManager(m_pZoneInfoManager);
    m_pZoneGroupInfoManager = new ZoneGroupInfoManager();
    de::loginContext().setZoneGroupInfoManager(m_pZoneGroupInfoManager);

    // create packet factory manager, packet validator
    // (They must be created and initialized before the client manager and the server-to-server manager.)
    m_pPacketFactoryManager = new PacketFactoryManager();
    de::kernelContext().setPacketFactoryManager(m_pPacketFactoryManager);
    m_pPacketValidator = new PacketValidator();
    de::kernelContext().setPacketValidator(m_pPacketValidator);

    // create inter-server communication manager
    m_pGameServerManager = new GameServerManager();
    de::loginContext().setGameServerManager(m_pGameServerManager);

    // create client manager
    m_pClientManager = new ClientManager();

    // create ItemDestroyer
    m_pItemDestroyer = new ItemDestroyer();

    // create UserInfoManager
    m_pUserInfoManager = new UserInfoManager();
    de::loginContext().setUserInfoManager(m_pUserInfoManager);

    // create GameWorldInfoManager
    m_pGameWorldInfoManager = new GameWorldInfoManager();
    de::serverContext().setGameWorldInfoManager(m_pGameWorldInfoManager);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// destructor
//
// The system manager's destructor must delete the sub-manager objects.
//
//////////////////////////////////////////////////////////////////////
LoginServer::~LoginServer() noexcept(false) {
    __BEGIN_TRY

    if (m_pClientManager != NULL) {
        delete m_pClientManager;
        m_pClientManager = NULL;
    }

    if (m_pItemDestroyer != NULL) {
        delete m_pItemDestroyer;
        m_pItemDestroyer = NULL;
    }

    if (m_pGameServerManager != NULL) {
        delete m_pGameServerManager;
        m_pGameServerManager = NULL;
    }

    if (m_pPacketValidator != NULL) {
        delete m_pPacketValidator;
        m_pPacketValidator = NULL;
    }

    if (m_pPacketFactoryManager != NULL) {
        delete m_pPacketFactoryManager;
        m_pPacketFactoryManager = NULL;
    }

    if (m_pZoneGroupInfoManager != NULL) {
        delete m_pZoneGroupInfoManager;
        m_pZoneGroupInfoManager = NULL;
    }

    if (m_pZoneInfoManager != NULL) {
        delete m_pZoneInfoManager;
        m_pZoneInfoManager = NULL;
    }

    if (m_pGameServerInfoManager != NULL) {
        delete m_pGameServerInfoManager;
        m_pGameServerInfoManager = NULL;
    }

    if (m_pGameServerGroupInfoManager != NULL) {
        delete m_pGameServerGroupInfoManager;
        m_pGameServerGroupInfoManager = NULL;
    }
    if (m_pDatabaseManager != NULL) {
        delete m_pDatabaseManager;
        m_pDatabaseManager = NULL;
    }
    if (m_pUserInfoManager != NULL) {
        delete m_pUserInfoManager;
        m_pUserInfoManager = NULL;
    }
    if (m_pGameWorldInfoManager != NULL) {
        delete m_pGameWorldInfoManager;
        m_pGameWorldInfoManager = NULL;
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// initialize game server
//
//////////////////////////////////////////////////////////////////////
void LoginServer::init() {
    __BEGIN_TRY

    // Initialize the database manager.
    m_pDatabaseManager->init();

    // initialize some info managers
    m_pGameServerInfoManager->init();
    m_pGameServerGroupInfoManager->init();
    m_pZoneInfoManager->init();
    m_pZoneGroupInfoManager->init();

    m_pGameWorldInfoManager->init();

    // Initialize the packet factory manager / packet validator before the client manager.
    m_pPacketFactoryManager->init();
    m_pPacketValidator->init();

    m_pUserInfoManager->init();

    // Initialize the server-to-server communication manager.
    m_pGameServerManager->init();

    // Once everything is ready, initialize the client manager and so
    // be ready for networking.
    m_pClientManager->init();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// start login server
//
//////////////////////////////////////////////////////////////////////
void LoginServer::start() {
    __BEGIN_TRY

    // Start the server-to-server communication manager.
    m_pGameServerManager->start();

    //
    // Start the client manager.
    //
    // *Reiot's Notes*
    //
    // It must run last, because it is not multi-thread based but
    // a function with an infinite loop. If another function were
    // called after it, then unless the loop ends (that is, unless an error occurs)
    // the other managers' processing loops would never run.
    //
    m_pClientManager->start();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// stop login server
//
// Mind the stop order: the manager with the widest reach goes first.
// Stopping in the opposite order can leave another manager dereferencing
// something that is already gone.
//
//////////////////////////////////////////////////////////////////////
void LoginServer::stop() {
    if (m_Stopped)
        return;
    __BEGIN_TRY

    // Stop the client manager first so no further connection is accepted.
    ServerShutdown::request();
    m_pClientManager->stop();

    // Request the stop before joining, then join while every manager the
    // worker uses (config, database, packet factory) is still alive.
    const de::ServerWorker workers[] = {{*m_pGameServerManager, "GameServerManager"}};
    de::stopServerWorkers(workers, cerr);
    m_Stopped = true;

    __END_CATCH
}
