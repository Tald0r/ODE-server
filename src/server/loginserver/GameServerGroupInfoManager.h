// The login server's group catalogue, loaded before request processing.
#ifndef __LOGIN_SERVER_GAME_SERVER_GROUP_INFO_MANAGER_H__
#define __LOGIN_SERVER_GAME_SERVER_GROUP_INFO_MANAGER_H__

#include <vector>

#include <unordered_map>

#include "GameServerGroupInfo.h"

class LoginConfigRepository;

// Load with quiescent readers. Failure retains the previous catalogue;
// successful replacement invalidates borrowed row pointers.
class GameServerGroupInfoManager {
public:
    GameServerGroupInfoManager() = default;
    GameServerGroupInfoManager(const GameServerGroupInfoManager&) = delete;
    GameServerGroupInfoManager& operator=(const GameServerGroupInfoManager&) = delete;

    void init();
    void load();
    void load(LoginConfigRepository& repository);

    const GameServerGroupInfo* getGameServerGroupInfo(ServerGroupID_t groupID, WorldID_t worldID) const;
    uint getSize(WorldID_t worldID) const noexcept;
    string toString() const;

private:
    using GroupTable = std::unordered_map<ServerGroupID_t, GameServerGroupInfo>;
    std::vector<GroupTable> m_Groups;
};

#endif
