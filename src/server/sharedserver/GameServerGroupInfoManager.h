//----------------------------------------------------------------------
// Filename    : GameServerGroupInfoManager.h
// Written By  : reiot@ewestsoft.com
// Description : The shared server's catalogue of game-server groups.
//----------------------------------------------------------------------

#ifndef __SHARED_SERVER_GAME_SERVER_GROUP_INFO_MANAGER_H__
#define __SHARED_SERVER_GAME_SERVER_GROUP_INFO_MANAGER_H__

#include <vector>

#include <unordered_map>

#include "GameServerGroupInfo.h"

class SharedConfigRepository;

// Load with quiescent readers. Failed loads preserve the current catalogue;
// successful replacement/destruction invalidates borrowed const row pointers.
class GameServerGroupInfoManager {
public:
    GameServerGroupInfoManager() = default;
    GameServerGroupInfoManager(const GameServerGroupInfoManager&) = delete;
    GameServerGroupInfoManager& operator=(const GameServerGroupInfoManager&) = delete;

    void init();
    void load();
    void load(SharedConfigRepository& repository);

    const GameServerGroupInfo* getGameServerGroupInfo(ServerGroupID_t groupID, WorldID_t worldID) const;
    uint getSize(WorldID_t worldID) const;
    string toString() const;

private:
    using Groups = std::unordered_map<ServerGroupID_t, GameServerGroupInfo>;
    std::vector<Groups> m_Groups;
};

#endif
