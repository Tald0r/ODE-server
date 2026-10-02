#ifndef __FAKE_WORLD_TOPOLOGY_H__
#define __FAKE_WORLD_TOPOLOGY_H__

// A WorldSelectionTopology over seeded tables that records every query in
// order, so a test can pin the lookups a decision makes - and the ones it
// does not.

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "WorldSelection.h"

class FakeWorldTopology : public WorldSelectionTopology {
public:
    // One configured group: what the group table says and how many accounts
    // it carries.
    struct Group {
        ServerGroupRow row;
        UserNum_t userNum = 0;
    };

    // Real membership, including each world's status.
    std::map<WorldID_t, WorldStatus> worlds = {{1, WORLD_OPEN}};
    // The groups of the world under test, keyed by their row's group ID.
    std::vector<Group> groups;

    // Every query, in the order it was made.
    std::vector<std::string> calls;

    void addGroup(ServerGroupID_t groupID, const std::string& name, BYTE stat, UserNum_t userNum) {
        Group group;
        group.row.groupID = groupID;
        group.row.groupName = name;
        group.row.stat = stat;
        group.userNum = userNum;
        groups.push_back(group);
    }

    std::vector<WorldID_t> worldIDs() override {
        calls.push_back("worldIDs");
        std::vector<WorldID_t> ids;
        for (const auto& [id, status] : worlds)
            ids.push_back(id);
        return ids;
    }

    WorldStatus worldStatus(WorldID_t worldID) override {
        calls.push_back("worldStatus(" + std::to_string((int)worldID) + ")");

        return worlds.at(worldID);
    }

    std::vector<ServerGroupID_t> serverGroupIDs(WorldID_t worldID) override {
        calls.push_back("serverGroupIDs(" + std::to_string((int)worldID) + ")");
        std::vector<ServerGroupID_t> ids;
        for (const auto& group : groups)
            ids.push_back(group.row.groupID);
        return ids;
    }

    ServerGroupRow serverGroup(ServerGroupID_t groupID, WorldID_t worldID) override {
        calls.push_back("serverGroup(" + std::to_string((int)groupID) + "," + std::to_string((int)worldID) + ")");
        return at(groupID).row;
    }

    UserNum_t serverGroupUserNum(ServerGroupID_t groupID, WorldID_t worldID) override {
        calls.push_back("serverGroupUserNum(" + std::to_string((int)groupID) + "," + std::to_string((int)worldID) +
                        ")");
        return at(groupID).userNum;
    }

private:
    const Group& at(ServerGroupID_t groupID) const {
        const auto found = std::find_if(groups.begin(), groups.end(),
                                        [groupID](const Group& group) { return group.row.groupID == groupID; });
        if (found == groups.end())
            throw std::out_of_range("Missing fake group");
        return *found;
    }
};

#endif
