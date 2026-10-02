//////////////////////////////////////////////////////////////////////////////
// Filename    : LoginWorldTopology.h
// Description : the world and server-group decisions' view of the tables the
//               login server loaded at startup.
//////////////////////////////////////////////////////////////////////////////

#ifndef __LOGIN_WORLD_TOPOLOGY_H__
#define __LOGIN_WORLD_TOPOLOGY_H__

#include "GameServerGroupInfoManager.h"
#include "GameWorldInfoManager.h"
#include "UserInfoManager.h"
#include "WorldSelection.h"

// Borrows supplied managers, which must outlive this view. Membership and rows
// are read on demand, so quiescent reloads and live population updates are visible.
// A missing configured row still throws NoSuchElementException.
class LoginWorldTopology : public WorldSelectionTopology {
public:
    LoginWorldTopology(const GameWorldInfoManager& worlds, const GameServerGroupInfoManager& groups,
                       const UserInfoManager& users)
        : m_Worlds(worlds), m_Groups(groups), m_Users(users) {}

    std::vector<WorldID_t> worldIDs() override {
        std::vector<WorldID_t> ids;
        ids.reserve(m_Worlds.getSize());
        for (const auto& [id, world] : m_Worlds.getGameWorldInfos())
            ids.push_back(id);
        return ids;
    }

    WorldStatus worldStatus(WorldID_t worldID) override {
        return m_Worlds.getGameWorldInfo(worldID)->getStatus();
    }

    std::vector<ServerGroupID_t> serverGroupIDs(WorldID_t worldID) override {
        return m_Groups.getGameServerGroupIDs(worldID);
    }

    ServerGroupRow serverGroup(ServerGroupID_t groupID, WorldID_t worldID) override {
        const GameServerGroupInfo* pGameServerGroupInfo = m_Groups.getGameServerGroupInfo(groupID, worldID);

        ServerGroupRow row;
        row.groupID = pGameServerGroupInfo->getGroupID();
        row.groupName = pGameServerGroupInfo->getGroupName();
        row.stat = pGameServerGroupInfo->getStat();
        return row;
    }

    UserNum_t serverGroupUserNum(ServerGroupID_t groupID, WorldID_t worldID) override {
        return m_Users.getUserInfo(groupID, worldID)->getUserNum();
    }

private:
    const GameWorldInfoManager& m_Worlds;
    const GameServerGroupInfoManager& m_Groups;
    const UserInfoManager& m_Users;
};

#endif
