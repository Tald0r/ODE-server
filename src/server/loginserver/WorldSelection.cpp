//////////////////////////////////////////////////////////////////////////////
// Filename    : WorldSelection.cpp
// Description : the world and server-group decisions behind
//               CLSelectWorldHandler and CLSelectServerHandler, and the
//               population ladder CLGetServerListHandler shares with them.
//////////////////////////////////////////////////////////////////////////////

#include "WorldSelection.h"

#include <algorithm>

BYTE serverGroupStatusFor(UserNum_t userNum, BYTE groupStat, const ServerLoadThresholds& thresholds) {
    const int population = userNum;

    BYTE stat = SERVER_FULL;

    if (population < thresholds.freeBelow + thresholds.userModify) {
        stat = SERVER_FREE;
    } else if (population < thresholds.normalBelow + thresholds.userModify) {
        stat = SERVER_NORMAL;
    } else if (population < thresholds.busyBelow + thresholds.userModify) {
        stat = SERVER_BUSY;
    } else if (population < thresholds.veryBusyBelow + thresholds.userModify) {
        stat = SERVER_VERY_BUSY;
    }

    // The absolute cap overrides the ladder.
    if (population >= thresholds.userMax)
        stat = SERVER_FULL;

    // A group the table calls down is shown as down whatever it is carrying.
    if (groupStat == SERVER_DOWN)
        stat = SERVER_DOWN;

    return stat;
}

std::vector<ServerListEntry> serverListFor(WorldID_t worldID, const ServerLoadThresholds& thresholds,
                                           ServerListTopology& topology) {
    auto groupIDs = topology.serverGroupIDs(worldID);
    std::sort(groupIDs.begin(), groupIDs.end());

    std::vector<ServerListEntry> entries;
    entries.reserve(groupIDs.size());

    for (const auto groupID : groupIDs) {
        const ServerGroupRow row = topology.serverGroup(groupID, worldID);

        ServerListEntry entry;
        entry.groupID = row.groupID;
        entry.groupName = row.groupName;
        entry.stat = serverGroupStatusFor(topology.serverGroupUserNum(row.groupID, worldID), row.stat, thresholds);

        entries.push_back(entry);
    }

    return entries;
}

Outcome<void, SelectWorldRejection> decideSelectWorld(WorldID_t worldID, WorldSelectionTopology& topology) {
    typedef Outcome<void, SelectWorldRejection> Result;

    const auto worldIDs = topology.worldIDs();
    const int worldCount = static_cast<int>(worldIDs.size());

    if (std::find(worldIDs.begin(), worldIDs.end(), worldID) == worldIDs.end()) {
        SelectWorldRejection rejection;
        rejection.reason = SelectWorldReason::UnknownWorld;
        rejection.worldCount = worldCount;
        return Result::Rejected(rejection);
    }

    if (topology.worldStatus(worldID) == WORLD_CLOSE) {
        SelectWorldRejection rejection;
        rejection.reason = SelectWorldReason::WorldClosed;
        rejection.worldCount = worldCount;
        return Result::Rejected(rejection);
    }

    return Result::Ok();
}

Outcome<SelectedServer, SelectServerRejection> decideSelectServer(const SelectServerRequest& request,
                                                                  WorldSelectionTopology& topology) {
    typedef Outcome<SelectedServer, SelectServerRejection> Result;

    const auto worldIDs = topology.worldIDs();
    if (worldIDs.empty())
        return Result::Rejected({SelectServerReason::NoWorlds, request.serverGroupID});

    SelectedServer selected;
    selected.worldID = request.worldID;
    if (std::find(worldIDs.begin(), worldIDs.end(), request.worldID) == worldIDs.end())
        selected.worldID = *std::max_element(worldIDs.begin(), worldIDs.end());

    const auto groupIDs = topology.serverGroupIDs(selected.worldID);
    if (groupIDs.empty())
        return Result::Rejected({SelectServerReason::NoServerGroups, request.serverGroupID});

    selected.serverGroupID = request.serverGroupID;
    if (std::find(groupIDs.begin(), groupIDs.end(), request.serverGroupID) == groupIDs.end())
        selected.serverGroupID = *std::max_element(groupIDs.begin(), groupIDs.end());

    if (topology.serverGroup(selected.serverGroupID, selected.worldID).stat == SERVER_DOWN) {
        SelectServerRejection rejection;
        rejection.reason = SelectServerReason::ServerClosed;
        rejection.serverGroupID = selected.serverGroupID;
        return Result::Rejected(rejection);
    }

    return Result::Ok(selected);
}
