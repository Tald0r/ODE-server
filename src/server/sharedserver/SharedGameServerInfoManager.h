//////////////////////////////////////////////////////////////////////////////
// Filename    : SharedGameServerInfoManager.h
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __SHARED_SERVER_GAME_SERVER_INFO_MANAGER_H__
#define __SHARED_SERVER_GAME_SERVER_INFO_MANAGER_H__

#include <unordered_map>

#include "Exception.h"
#include "SharedGameServerInfo.h"
#include "Types.h"

typedef unordered_map<ServerID_t, SharedGameServerInfo*> HashMapSharedGameServerInfo;
typedef HashMapSharedGameServerInfo::iterator HashMapSharedGameServerInfoItor;

//////////////////////////////////////////////////////////////////////////////
// class SharedGameServerInfoManager;
// Holds an unordered_map of SharedGameServerInfo keyed by the game server ID
// internally.
// Holds the SharedGameServerInfo of a single World.
//////////////////////////////////////////////////////////////////////////////

class SharedGameServerInfoManager {
public:
    SharedGameServerInfoManager();
    ~SharedGameServerInfoManager();

public:
    void init();
    void load();

    void addGameServerInfo(SharedGameServerInfo* pGameServerInfo, const ServerGroupID_t ServerGroupID);
    void deleteGameServerInfo(const ServerID_t ServerID, const ServerGroupID_t ServerGroupID);
    SharedGameServerInfo* getGameServerInfo(const ServerID_t ServerID, const ServerGroupID_t ServerGroupID) const;
    uint getSize(const ServerGroupID_t ServerGroupID) const {
        return m_pGameServerInfos[ServerGroupID].size();
    }
    string toString() const;

    // get MaxServerGroupID
    int getMaxServerGroupID() const {
        return m_MaxServerGroupID;
    }

private:
    // hash map of SharedGameServerInfo
    // key   : GameServerID_t
    // value : SharedGameServerInfo *
    HashMapSharedGameServerInfo* m_pGameServerInfos = nullptr;
    int m_MaxServerGroupID = 0;
};

#endif
