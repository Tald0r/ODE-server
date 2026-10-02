//////////////////////////////////////////////////////////////////////////////
// Filename    : GlobalWorldTopology.h
// Description : the world and server-group decisions' view of the tables the
//               login server loaded at startup.
//////////////////////////////////////////////////////////////////////////////

#ifndef __GLOBAL_WORLD_TOPOLOGY_H__
#define __GLOBAL_WORLD_TOPOLOGY_H__

#include "GameServerGroupInfoManager.h"
#include "GameWorldInfoManager.h"
#include "LoginContext.h"
#include "ServerContext.h"
#include "UserInfoManager.h"
#include "WorldSelection.h"

// Each lookup is the manager call the handlers used to make inline, so a
// missing row still throws NoSuchElementException where it did.
class GlobalWorldTopology : public WorldSelectionTopology {
public:
    int worldCount() override {
        return de::serverContext().worldInfos().getSize();
    }

    WorldStatus worldStatus(WorldID_t worldID) override {
        return de::serverContext().worldInfos().getGameWorldInfo(worldID)->getStatus();
    }

    int serverGroupCount(WorldID_t worldID) override {
        return de::loginContext().gameServerGroups().getSize(worldID);
    }

    ServerGroupRow serverGroup(ServerGroupID_t groupID, WorldID_t worldID) override {
        const GameServerGroupInfo* pGameServerGroupInfo =
            de::loginContext().gameServerGroups().getGameServerGroupInfo(groupID, worldID);

        ServerGroupRow row;
        row.groupID = pGameServerGroupInfo->getGroupID();
        row.groupName = pGameServerGroupInfo->getGroupName();
        row.stat = pGameServerGroupInfo->getStat();
        return row;
    }

    UserNum_t serverGroupUserNum(ServerGroupID_t groupID, WorldID_t worldID) override {
        return de::loginContext().userInfos().getUserInfo(groupID, worldID)->getUserNum();
    }
};

#endif
