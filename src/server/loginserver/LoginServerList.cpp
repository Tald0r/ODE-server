#include "LoginServerList.h"

#include <memory>
#include <utility>

#include "LCServerList.h"
#include "Player.h"
#include "ServerGroupInfo.h"
#include "repository/LoginAccountRepository.h"

namespace de {

void sendLoginServerList(Player& player, WorldID_t worldID, ServerListTopology& topology,
                         LoginAccountRepository& accounts, LoginServerListQuery query,
                         const ServerLoadThresholds& thresholds) {
    const auto groups = serverListFor(worldID, thresholds, topology);
    if (groups.size() > ServerGroupInfo::kMaxCount)
        throw InvalidProtocolException("too many server group infos");

    LCServerList reply;
    int currentServerGroupID = 0;
    bool found;
    if (query == LoginServerListQuery::CurrentGroup) {
        found = accounts.loadCurrentServerGroup(player.getID(), currentServerGroupID);
    } else {
        int currentWorldID = 0;
        found = accounts.loadCurrentLocation(LOGIN_LOCATION_SQL_LOWER, player.getID(), currentWorldID,
                                             currentServerGroupID);
    }
    if (found) {
        if (!std::in_range<ServerGroupID_t>(currentServerGroupID))
            throw Error("invalid current server group ID");
        reply.setCurrentServerGroupID(static_cast<ServerGroupID_t>(currentServerGroupID));
    }
    for (const auto& group : groups) {
        auto info = std::make_unique<ServerGroupInfo>();
        info->setGroupID(group.groupID);
        info->setGroupName(group.groupName);
        info->setStat(group.stat);
        reply.addListElement(info.release()); // Consumed even if list insertion fails.
    }
    player.sendPacket(&reply);
}

} // namespace de
