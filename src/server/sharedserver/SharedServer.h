//////////////////////////////////////////////////////////////////////
//
// Filename    : SharedServer.h
// Written By  : reiot@ewestsoft.com
// Description : Main class for the login server
//
//////////////////////////////////////////////////////////////////////

#ifndef __SHARED_SERVER_H__
#define __SHARED_SERVER_H__

// Including this module makes it the login server module.
#ifndef __SHARED_SERVER__
#define __SHARED_SERVER__
#endif

// include files
#include <memory>

#include "Exception.h"
#include "Types.h"

class DatabaseManager;
class GameServerGroupInfoManager;
class GameServerManager;
class GameWorldInfoManager;
class GuildManager;
class HeartbeatManager;
class PacketFactoryManager;
class PacketValidator;
class ResurrectLocationManager;
class SharedGameServerInfoManager;
class ServerSocket;
class StringPool;

//////////////////////////////////////////////////////////////////////
//
// class SharedServer
//
// Class representing the login server itself.
//
//////////////////////////////////////////////////////////////////////

class SharedServer {
public:
    // Construct all managers before publishing their context bindings. A
    // supplied owned listener permits scoped use without configuration binding;
    // a null listener selects the normal listener built from configuration.
    // Construction/destruction require quiescent context users and nested
    // server scopes must unwind in reverse order. Destroy outside the worker.
    SharedServer();
    explicit SharedServer(std::unique_ptr<ServerSocket> listener);

    // Join the owned worker before restoring bindings or releasing dependencies.
    ~SharedServer() noexcept;
    SharedServer(const SharedServer&) = delete;
    SharedServer& operator=(const SharedServer&) = delete;

    // intialize game server
    void init();

    // start game server
    void start();

    // Request every worker to stop, then join them while the managers they
    // use are still alive. Idempotent: main and the startup catch both call it.
    void stop();

private:
    bool m_Stopped = false;

    // Dependencies precede workers so failed construction also destroys the
    // workers first. Contexts only borrow these completed owners.
    std::unique_ptr<DatabaseManager> m_pDatabaseManager;
    std::unique_ptr<GameWorldInfoManager> m_pGameWorldInfoManager;
    std::unique_ptr<GuildManager> m_pGuildManager;
    std::unique_ptr<SharedGameServerInfoManager> m_pGameServerInfoManager;
    std::unique_ptr<GameServerGroupInfoManager> m_pGameServerGroupInfoManager;
    std::unique_ptr<PacketFactoryManager> m_pPacketFactoryManager;
    std::unique_ptr<PacketValidator> m_pPacketValidator;
    std::unique_ptr<ResurrectLocationManager> m_pResurrectLocationManager;
    std::unique_ptr<StringPool> m_pStringPool;
    std::unique_ptr<GameServerManager> m_pGameServerManager;
    std::unique_ptr<HeartbeatManager> m_pHeartbeatManager;

    DatabaseManager* m_PreviousDatabaseManager = nullptr;
    GameWorldInfoManager* m_PreviousGameWorldInfoManager = nullptr;
    GuildManager* m_PreviousGuildManager = nullptr;
    PacketFactoryManager* m_PreviousPacketFactoryManager = nullptr;
    PacketValidator* m_PreviousPacketValidator = nullptr;
    StringPool* m_PreviousStringPool = nullptr;
    GameServerManager* m_PreviousGameServerManager = nullptr;
};

#endif
