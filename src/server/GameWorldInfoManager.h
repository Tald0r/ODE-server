//----------------------------------------------------------------------
// Filename    : GameWorldInfoManager.h
// Written By  : reiot@ewestsoft.com
// Description : The world catalogue shared by all three servers.
//----------------------------------------------------------------------

#ifndef __GAME_WORLD_INFO_MANAGER_H__
#define __GAME_WORLD_INFO_MANAGER_H__

#include <unordered_map>

#include "GameWorldInfo.h"

class ServerInfoRepository;

// Load with quiescent readers. Failed loads retain the previous catalogue;
// successful replacement/destruction invalidates borrowed const row pointers.
class GameWorldInfoManager {
public:
    GameWorldInfoManager() = default;
    GameWorldInfoManager(const GameWorldInfoManager&) = delete;
    GameWorldInfoManager& operator=(const GameWorldInfoManager&) = delete;

    void init();
    void load();
    void load(ServerInfoRepository& repository);

    const GameWorldInfo* getGameWorldInfo(WorldID_t worldID) const;
    uint getSize() const {
        return m_GameWorldInfos.size();
    }
    string toString() const;

private:
    std::unordered_map<WorldID_t, GameWorldInfo> m_GameWorldInfos;
};

#endif
