//////////////////////////////////////////////////////////////////////
//
// Filename    : SharedServer.cpp
// Written By  : reiot@ewestsoft.com
// Description : Main class for the shared server
//
//////////////////////////////////////////////////////////////////////

// include files
#include "SharedServer.h"

#include <memory>
#include <utility>

#include "Assert.h"
#include "GameServerGroupInfoManager.h"
#include "GameServerManager.h"
#include "GameWorldInfoManager.h"
#include "GuildManager.h"
#include "HeartbeatManager.h"
#include "KernelContext.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "ResurrectLocationManager.h"
#include "ServerContext.h"
#include "ServerShutdown.h"
#include "ServerSocket.h"
#include "ServerStartSequence.h"
#include "ServerWorkerShutdown.h"
#include "SharedContext.h"
#include "SharedGameServerInfoManager.h"
#include "StringPool.h"
#include "database/DatabaseManager.h"
#include "types/ServerType.h"

//////////////////////////////////////////////////////////////////////
//
// constructor
//
// The system manager's constructor creates the sub-manager objects.
//
//////////////////////////////////////////////////////////////////////
SharedServer::SharedServer() : SharedServer(nullptr) {}

SharedServer::SharedServer(std::unique_ptr<ServerSocket> listener) {
    __BEGIN_TRY

    // create database manager
    m_pDatabaseManager = std::make_unique<DatabaseManager>();

    // create guild manager
    m_pGuildManager = std::make_unique<GuildManager>();

    // create some info managers
    m_pGameServerInfoManager = std::make_unique<SharedGameServerInfoManager>();
    m_pGameServerGroupInfoManager = std::make_unique<GameServerGroupInfoManager>();

    // create packet factory manager, packet validator
    // (They must be created and initialized before the client manager and the server-to-server manager.)
    m_pPacketFactoryManager = std::make_unique<PacketFactoryManager>();
    m_pPacketValidator = std::make_unique<PacketValidator>();

    // create inter-server communication manager
    m_pGameServerManager =
        listener ? std::make_unique<GameServerManager>(std::move(listener)) : std::make_unique<GameServerManager>();

    // create client manager
    m_pHeartbeatManager = std::make_unique<HeartbeatManager>();

    // create GameWorldInfoManager
    m_pGameWorldInfoManager = std::make_unique<GameWorldInfoManager>();

    // create ResurrectLocationManager
    m_pResurrectLocationManager = std::make_unique<ResurrectLocationManager>();

    m_pStringPool = std::make_unique<StringPool>();

    // All constructors have completed. These exchanges cannot throw, so a
    // failed construction never publishes any part of the graph.
    auto& server = de::serverContext();
    auto& kernel = de::kernelContext();
    auto& shared = de::sharedContext();
    m_PreviousDatabaseManager = server.exchangeDatabaseManager(m_pDatabaseManager.get());
    m_PreviousGameWorldInfoManager = server.exchangeGameWorldInfoManager(m_pGameWorldInfoManager.get());
    m_PreviousPacketFactoryManager = kernel.exchangePacketFactoryManager(m_pPacketFactoryManager.get());
    m_PreviousPacketValidator = kernel.exchangePacketValidator(m_pPacketValidator.get());
    m_PreviousGuildManager = shared.exchangeGuildManager(m_pGuildManager.get());
    m_PreviousGameServerManager = shared.exchangeGameServerManager(m_pGameServerManager.get());
    m_PreviousStringPool = shared.exchangeStringPool(m_pStringPool.get());

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// destructor
//
// The system manager's destructor must delete the sub-manager objects.
//
//////////////////////////////////////////////////////////////////////
SharedServer::~SharedServer() noexcept {
    m_pHeartbeatManager.reset();
    m_pGameServerManager.reset(); // Its destructor stops/joins while dependencies are live.

    auto& server = de::serverContext();
    auto& kernel = de::kernelContext();
    auto& shared = de::sharedContext();
    (void)shared.exchangeStringPool(m_PreviousStringPool);
    (void)shared.exchangeGameServerManager(m_PreviousGameServerManager);
    (void)shared.exchangeGuildManager(m_PreviousGuildManager);
    (void)kernel.exchangePacketValidator(m_PreviousPacketValidator);
    (void)kernel.exchangePacketFactoryManager(m_PreviousPacketFactoryManager);
    (void)server.exchangeGameWorldInfoManager(m_PreviousGameWorldInfoManager);
    (void)server.exchangeDatabaseManager(m_PreviousDatabaseManager);
}


//////////////////////////////////////////////////////////////////////
//
// initialize game server
//
//////////////////////////////////////////////////////////////////////
void SharedServer::init() {
    __BEGIN_TRY

    cout << "SharedServer::init() start" << endl;

    // Initialize the database manager.
    m_pDatabaseManager->init();

    m_pStringPool->load();

    // Initialize the guild manager.
    m_pGuildManager->init();

    // initialize some info managers
    m_pGameServerInfoManager->init();
    m_pGameServerGroupInfoManager->init();

    m_pGameWorldInfoManager->init();

    // Initialize the packet factory manager / packet validator before the client manager.
    m_pPacketFactoryManager->init();
    m_pPacketValidator->init();

    // Initialize the server-to-server communication manager.
    m_pGameServerManager->init();

    // ResurrectLocationManager initialization
    m_pResurrectLocationManager->init();

    // Once everything is ready, initialize the client manager and so
    // be ready for networking.
    m_pHeartbeatManager->init();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// start shared server
//
//////////////////////////////////////////////////////////////////////
void SharedServer::start() {
    __BEGIN_TRY

    cout << "---------- Start SharedServer ---------" << endl;
    const de::ServerStartAction backgroundStarts[] = {[this] { m_pGameServerManager->start(); }};
    de::runServerStartSequence(backgroundStarts, [this] { m_pHeartbeatManager->start(); });

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// stop shared server
//
// Mind the stop order: the manager with the widest reach goes first.
// Stopping in the opposite order can leave another manager dereferencing
// something that is already gone.
//
//////////////////////////////////////////////////////////////////////
void SharedServer::stop() {
    if (m_Stopped)
        return;
    __BEGIN_TRY

    // End the main-thread heartbeat loop first, so nothing new is started.
    ServerShutdown::request();
    m_pHeartbeatManager->stop();

    // Request the stop before joining, then join while every manager the
    // worker uses (config, database, guild manager) is still alive.
    const de::ServerWorker workers[] = {{*m_pGameServerManager, "GameServerManager"}};
    de::stopServerWorkers(workers, cerr);
    m_Stopped = true;

    __END_CATCH
}
