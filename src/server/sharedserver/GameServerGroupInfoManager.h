//----------------------------------------------------------------------
//
// Filename    : GameServerGroupInfoManager.h
// Written By  : reiot@ewestsoft.com
// Description :
//
//----------------------------------------------------------------------

#ifndef __SHARED_SERVER_GAME_SERVER_GROUP_INFO_MANAGER_H__
#define __SHARED_SERVER_GAME_SERVER_GROUP_INFO_MANAGER_H__

// include files
#include <unordered_map>

#include "Exception.h"
#include "GameServerGroupInfo.h"
#include "Types.h"

typedef unordered_map<ServerGroupID_t, GameServerGroupInfo*> HashMapGameServerGroupInfo;

//----------------------------------------------------------------------
//
// class GameServerGroupInfoManager;
//
// Holds an unordered_map of GameServerGroupInfo keyed by the game server ID
// internally.
//
//----------------------------------------------------------------------

class GameServerGroupInfoManager {
public:
    // constructor
    GameServerGroupInfoManager();

    // destructor
    ~GameServerGroupInfoManager();

    // initialize manager
    void init();

    // load from database
    void load();

    // add info
    void addGameServerGroupInfo(GameServerGroupInfo* pGameServerGroupInfo, WorldID_t WorldID);

    // delete info
    void deleteGameServerGroupInfo(const ServerGroupID_t ServerGroupID, WorldID_t WorldID);

    // get GameServerGroupInfo by ServerGroupID
    GameServerGroupInfo* getGameServerGroupInfo(const ServerGroupID_t ServerGroupID, WorldID_t WorldID) const;

    // get count of info
    uint getSize(WorldID_t WorldID) const {
        return m_GameServerGroupInfos[WorldID].size();
    }

    // get debug string
    string toString() const;

private:
    // hash map of GameServerGroupInfo
    // key   : GameServerGroupID_t
    // value : GameServerGroupInfo *
    HashMapGameServerGroupInfo* m_GameServerGroupInfos = nullptr;

    WorldID_t m_MaxWorldID = 0;
};

#endif
