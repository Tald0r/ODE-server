//////////////////////////////////////////////////////////////////////////////
// Filename    : SharedContext.h
// Description : The managers a sharedserver subsystem works against, handed to
//               it explicitly instead of looked up through a global.
//
//               The context does NOT own the managers: each is still created
//               and destroyed by the startup code that holds it (SharedServer),
//               which publishes its completed graph here and restores previous
//               bindings after joining its worker during destruction. A manager is
//               therefore null until its creation point is reached, and an
//               accessor asserts on a null one: reading a manager before it
//               exists is a startup-order bug, not a runtime condition to
//               branch on.
//
//               The manager class names are the shared server's own. The game
//               server and the login server declare classes named
//               GameServerManager, GuildManager and StringPool too; those are
//               different types in different executables, and this header
//               names only the shared server's.
//
//               Only forward declarations live here, so the header costs a
//               caller nothing and can be included where none of the managers
//               are linked.
//////////////////////////////////////////////////////////////////////////////

#ifndef __SHARED_CONTEXT_H__
#define __SHARED_CONTEXT_H__

class GameServerManager;
class GuildManager;
class StringPool;

namespace de {

class SharedContext {
public:
    SharedContext() = default;

    SharedContext(const SharedContext&) = delete;
    SharedContext& operator=(const SharedContext&) = delete;

    // Exchanges support scoped owners, including initially empty bindings.
    void setGameServerManager(GameServerManager* pGameServerManager) {
        m_pGameServerManager = pGameServerManager;
    }
    GameServerManager* exchangeGameServerManager(GameServerManager* manager) noexcept {
        GameServerManager* previous = m_pGameServerManager;
        m_pGameServerManager = manager;
        return previous;
    }
    void setGuildManager(GuildManager* pGuildManager) {
        m_pGuildManager = pGuildManager;
    }
    GuildManager* exchangeGuildManager(GuildManager* manager) noexcept {
        GuildManager* previous = m_pGuildManager;
        m_pGuildManager = manager;
        return previous;
    }
    void setStringPool(StringPool* pStringPool) {
        m_pStringPool = pStringPool;
    }
    StringPool* exchangeStringPool(StringPool* pool) noexcept {
        StringPool* previous = m_pStringPool;
        m_pStringPool = pool;
        return previous;
    }

    GameServerManager& gameServers() const;
    GuildManager& guilds() const;
    StringPool& strings() const;

private:
    GameServerManager* m_pGameServerManager = nullptr;
    GuildManager* m_pGuildManager = nullptr;
    StringPool* m_pStringPool = nullptr;
};

// The process-wide context the startup code fills. A converted subsystem is
// handed the context and never calls this; the call belongs at the boundary
// where a subsystem is created from code that still reads globals.
SharedContext& sharedContext();

} // namespace de

#endif
