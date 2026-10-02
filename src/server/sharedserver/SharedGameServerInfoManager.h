//////////////////////////////////////////////////////////////////////////////
// Filename    : SharedGameServerInfoManager.h
// Written By  : reiot@ewestsoft.com
// Description : The shared server's catalogue of one world's game servers.
//////////////////////////////////////////////////////////////////////////////

#ifndef __SHARED_SERVER_GAME_SERVER_INFO_MANAGER_H__
#define __SHARED_SERVER_GAME_SERVER_INFO_MANAGER_H__

#include <vector>

#include <unordered_map>

#include "SharedGameServerInfo.h"

class SharedConfigRepository;

// Load with quiescent readers. Failed loads preserve the current catalogue;
// successful replacement/destruction invalidates borrowed const row pointers.
class SharedGameServerInfoManager {
public:
    SharedGameServerInfoManager() = default;
    SharedGameServerInfoManager(const SharedGameServerInfoManager&) = delete;
    SharedGameServerInfoManager& operator=(const SharedGameServerInfoManager&) = delete;

    void init();
    void load();
    void load(SharedConfigRepository& repository, int worldID);

    const SharedGameServerInfo* getGameServerInfo(ServerID_t serverID, ServerGroupID_t groupID) const;
    uint getSize(ServerGroupID_t groupID) const;
    string toString() const;
    int getMaxServerGroupID() const {
        return static_cast<int>(m_Servers.size());
    }

private:
    using Servers = std::unordered_map<ServerID_t, SharedGameServerInfo>;
    std::vector<Servers> m_Servers;
};

#endif
