//////////////////////////////////////////////////////////////////////////////
// Filename    : LoginContext.h
// Description : The managers a loginserver subsystem works against, handed to
//               it explicitly instead of looked up through a global.
//
//               The context does NOT own the managers: each is still created
//               and destroyed by the startup code that holds it (LoginServer
//               for the server-wide ones, ClientManager and LoginPlayerManager
//               for the ones they create), and registers itself here as soon
//               as it exists. A manager is therefore null until its creation
//               point is reached, and an accessor asserts on a null one:
//               reading a manager before it exists is a startup-order bug, not
//               a runtime condition to branch on.
//
//               The manager class names are the loginserver's own. The game
//               server and the shared server declare classes named
//               GameServerGroupInfoManager, GameServerManager and
//               ZoneInfoManager too; those are different types in different
//               executables, and this header names only the loginserver's.
//
//               Only forward declarations live here, so the header costs a
//               caller nothing and can be included where none of the managers
//               are linked.
//////////////////////////////////////////////////////////////////////////////

#ifndef __LOGIN_CONTEXT_H__
#define __LOGIN_CONTEXT_H__

class GameServerGroupInfoManager;
class GameServerManager;
class LoginPlayerManager;
class ReconnectLoginInfoManager;
class UserInfoManager;
class ZoneGroupInfoManager;
class ZoneInfoManager;

namespace de {

class LoginContext {
public:
    LoginContext() = default;

    LoginContext(const LoginContext&) = delete;
    LoginContext& operator=(const LoginContext&) = delete;

    void setGameServerGroupInfoManager(GameServerGroupInfoManager* pGameServerGroupInfoManager) {
        m_pGameServerGroupInfoManager = pGameServerGroupInfoManager;
    }
    void setGameServerManager(GameServerManager* pGameServerManager) {
        m_pGameServerManager = pGameServerManager;
    }
    void setLoginPlayerManager(LoginPlayerManager* pLoginPlayerManager) {
        m_pLoginPlayerManager = pLoginPlayerManager;
    }
    void setReconnectLoginInfoManager(ReconnectLoginInfoManager* pReconnectLoginInfoManager) {
        m_pReconnectLoginInfoManager = pReconnectLoginInfoManager;
    }
    // Scoped owners restore the returned binding before destroying the manager
    // they published. Nested owners must leave scope in reverse creation order.
    ReconnectLoginInfoManager* exchangeReconnectLoginInfoManager(ReconnectLoginInfoManager* manager) noexcept {
        auto* previous = m_pReconnectLoginInfoManager;
        m_pReconnectLoginInfoManager = manager;
        return previous;
    }
    void setUserInfoManager(UserInfoManager* pUserInfoManager) {
        m_pUserInfoManager = pUserInfoManager;
    }
    void setZoneGroupInfoManager(ZoneGroupInfoManager* pZoneGroupInfoManager) {
        m_pZoneGroupInfoManager = pZoneGroupInfoManager;
    }
    void setZoneInfoManager(ZoneInfoManager* pZoneInfoManager) {
        m_pZoneInfoManager = pZoneInfoManager;
    }

    GameServerGroupInfoManager& gameServerGroups() const;
    GameServerManager& gameServers() const;
    LoginPlayerManager& loginPlayers() const;
    ReconnectLoginInfoManager& reconnectLogins() const;
    UserInfoManager& userInfos() const;
    ZoneGroupInfoManager& zoneGroupInfos() const;
    ZoneInfoManager& zoneInfos() const;

private:
    GameServerGroupInfoManager* m_pGameServerGroupInfoManager = nullptr;
    GameServerManager* m_pGameServerManager = nullptr;
    LoginPlayerManager* m_pLoginPlayerManager = nullptr;
    ReconnectLoginInfoManager* m_pReconnectLoginInfoManager = nullptr;
    UserInfoManager* m_pUserInfoManager = nullptr;
    ZoneGroupInfoManager* m_pZoneGroupInfoManager = nullptr;
    ZoneInfoManager* m_pZoneInfoManager = nullptr;
};

// The process-wide context the startup code fills. A converted subsystem is
// handed the context and never calls this; the call belongs at the boundary
// where a subsystem is created from code that still reads globals.
LoginContext& loginContext();

} // namespace de

#endif
