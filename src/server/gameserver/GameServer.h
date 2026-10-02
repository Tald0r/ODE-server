//////////////////////////////////////////////////////////////////////////////
// Filename    : GameServer.h
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __GAME_SERVER_H__
#define __GAME_SERVER_H__

#include "Exception.h"
#include "Properties.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////////////
// class GameServer
//////////////////////////////////////////////////////////////////////////////

class ClientManager;
class DatabaseManager;
class GameServerInfoManager;
class LoginServerManager;
class MPacketManager;
class MPlayerManager;
class ObjectManager;
class PacketFactoryManager;
class PacketValidator;
class SharedServerManager;
class ThreadManager;

class GameServer {
public:
    GameServer();
    ~GameServer();

public:
    void init();

    void start();

    void stop();

private:
    bool m_Stopped = false; // Lifecycle operations are owned by the main thread.
    void goBackground();

    // Managers this class creates and deletes. The client manager, the login
    // and shared server links and the mofus pair are registered on
    // de::GameContext, the packet factory table and the validator on
    // de::KernelContext and the database connection table on
    // de::ServerContext, the two registries every binary fills with a set of
    // its own; the other two are reached only from here. The mofus pair is created only
    // where the module is built, so its members stay null otherwise, as its
    // globals did.
    ClientManager* m_pClientManager = nullptr;
    DatabaseManager* m_pDatabaseManager = nullptr;
    GameServerInfoManager* m_pGameServerInfoManager = nullptr;
    LoginServerManager* m_pLoginServerManager = nullptr;
    MPacketManager* m_pMPacketManager = nullptr;
    MPlayerManager* m_pMPlayerManager = nullptr;
    ObjectManager* m_pObjectManager = nullptr;
    PacketFactoryManager* m_pPacketFactoryManager = nullptr;
    PacketValidator* m_pPacketValidator = nullptr;
    SharedServerManager* m_pSharedServerManager = nullptr;
    ThreadManager* m_pThreadManager = nullptr;
};

#endif
