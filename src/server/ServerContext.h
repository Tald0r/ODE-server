//////////////////////////////////////////////////////////////////////////////
// Filename    : ServerContext.h
// Description : The process-wide managers whose classes ServerCore defines,
//               handed to a caller explicitly instead of looked up through a
//               global.
//
//               These three are server-agnostic: the database connection
//               table, the world table and the game-server table are the same
//               classes in all three binaries, each of which creates a set of
//               its own, so they belong here rather than on any one server's
//               context. The game server creates all three, the login server
//               all three, the shared server the connection table and the
//               world table.
//
//               The context does NOT own the managers: each is still created
//               and destroyed by the startup code that holds it (each
//               server's server object, and the game server's ObjectManager
//               for the world table), and is registered when its owning startup
//               stage completes. A manager is null until its publication point
//               is reached, and an accessor asserts on a null one: reading a
//               manager before it exists is a startup-order bug, not a
//               runtime condition to branch on.
//
//               Only forward declarations live here, so the header costs a
//               caller nothing and can be included where none of the managers
//               are linked.
//////////////////////////////////////////////////////////////////////////////

#ifndef __SERVER_CONTEXT_H__
#define __SERVER_CONTEXT_H__

class DatabaseManager;
class GameServerInfoManager;
class GameWorldInfoManager;

namespace de {

class ServerContext {
public:
    ServerContext() = default;

    ServerContext(const ServerContext&) = delete;
    ServerContext& operator=(const ServerContext&) = delete;

    void setDatabaseManager(DatabaseManager* pDatabaseManager) {
        m_pDatabaseManager = pDatabaseManager;
    }
    DatabaseManager* exchangeDatabaseManager(DatabaseManager* manager) noexcept {
        DatabaseManager* previous = m_pDatabaseManager;
        m_pDatabaseManager = manager;
        return previous;
    }
    void setGameServerInfoManager(GameServerInfoManager* pGameServerInfoManager) {
        m_pGameServerInfoManager = pGameServerInfoManager;
    }
    void setGameWorldInfoManager(GameWorldInfoManager* pGameWorldInfoManager) {
        m_pGameWorldInfoManager = pGameWorldInfoManager;
    }
    GameWorldInfoManager* exchangeGameWorldInfoManager(GameWorldInfoManager* manager) noexcept {
        GameWorldInfoManager* previous = m_pGameWorldInfoManager;
        m_pGameWorldInfoManager = manager;
        return previous;
    }

    DatabaseManager& database() const;
    GameServerInfoManager& serverInfos() const;
    GameWorldInfoManager& worldInfos() const;

private:
    DatabaseManager* m_pDatabaseManager = nullptr;
    GameServerInfoManager* m_pGameServerInfoManager = nullptr;
    GameWorldInfoManager* m_pGameWorldInfoManager = nullptr;
};

// The process-wide context the startup code fills. A converted subsystem is
// handed the context and never calls this; the call belongs at the boundary
// where a subsystem is created from code that still reads globals.
ServerContext& serverContext();

} // namespace de

#endif
