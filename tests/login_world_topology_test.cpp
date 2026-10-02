#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "LoginWorldTopology.h"
#include "repository/LoginConfigRepository.h"
#include "repository/ServerInfoRepository.h"

namespace {

class TopologyRepository : public ServerInfoRepository, public LoginConfigRepository {
public:
    std::vector<ServerInfoWorldRow> loadWorlds() override {
        return worlds;
    }
    bool loadMaxGameServerGroupWorldID(int& maximum) override {
        maximum = 255;
        return true;
    }
    std::vector<LoginGameServerGroupRow> loadGameServerGroups() override {
        return groups;
    }
    std::vector<LoginGameServerGroupIDRow> loadGameServerGroupIDs() override {
        std::vector<LoginGameServerGroupIDRow> ids;
        for (const auto& group : groups)
            ids.push_back({group.worldID, group.groupID});
        return ids;
    }
    bool loadMaxServerGroupID(int&) override {
        std::abort();
    }
    bool loadMaxWorldID(int&) override {
        std::abort();
    }
    std::vector<ServerInfoRow> loadServers() override {
        std::abort();
    }
    std::vector<ServerInfoNonPKRow> loadNonPKServers() override {
        std::abort();
    }
    std::vector<ServerInfoCastleStatRow> loadCastleStats() override {
        std::abort();
    }
    std::vector<LoginZoneGroupRow> loadZoneGroups() override {
        std::abort();
    }
    std::vector<LoginZoneRow> loadZones() override {
        std::abort();
    }
    bool loadClientVersion(int&) override {
        std::abort();
    }

    std::vector<ServerInfoWorldRow> worlds = {{7, "Sparse world", WORLD_OPEN}, {1, "First world", WORLD_OPEN}};
    std::vector<LoginGameServerGroupRow> groups = {{7, 4, "Other world group", SERVER_NORMAL},
                                                   {1, 2, "A group name longer than the packet field", SERVER_FREE},
                                                   {1, 0, "First group", SERVER_FREE}};
};

class LoginWorldTopologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        reload();
    }
    void reload() {
        worlds.load(repository);
        groups.load(repository);
        users.load(repository);
    }

    TopologyRepository repository;
    GameWorldInfoManager worlds;
    GameServerGroupInfoManager groups;
    UserInfoManager users;
    LoginWorldTopology topology{worlds, groups, users};
};

TEST_F(LoginWorldTopologyTest, ReadsOnlyTheSuppliedManagersAndSeesLivePopulationUpdates) {
    EXPECT_EQ(topology.worldStatus(7), WORLD_OPEN);
    const auto row = topology.serverGroup(4, 7);
    EXPECT_EQ(row.groupID, 4);
    EXPECT_EQ(row.groupName, "Other world group");
    EXPECT_EQ(row.stat, SERVER_NORMAL);
    EXPECT_EQ(topology.serverGroupUserNum(4, 7), 0);
    users.getUserInfo(4, 7)->setUserNum(1234);
    EXPECT_EQ(topology.serverGroupUserNum(4, 7), 1234);
    EXPECT_EQ(topology.serverGroupUserNum(0, 1), 0);
}

TEST_F(LoginWorldTopologyTest, ExistingFirstWorldAndGroupRemainSelectable) {
    EXPECT_TRUE(decideSelectWorld(1, topology).isOk());
    const auto selected = decideSelectServer({1, 0}, topology);
    ASSERT_TRUE(selected.isOk());
    EXPECT_EQ(selected.events().worldID, 1);
    EXPECT_EQ(selected.events().serverGroupID, 0);
}

TEST_F(LoginWorldTopologyTest, AListedSparseWorldCanBeSelected) {
    EXPECT_TRUE(decideSelectWorld(7, topology).isOk());
}

TEST_F(LoginWorldTopologyTest, AMissingWorldBelowTheCountIsRefusedWithoutLookingItUp) {
    const auto selected = decideSelectWorld(2, topology);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection().reason, SelectWorldReason::UnknownWorld);
    EXPECT_EQ(selected.rejection().worldCount, 2);
}

TEST_F(LoginWorldTopologyTest, AClosedSparseWorldIsReportedAsClosed) {
    repository.worlds[0].stat = WORLD_CLOSE;
    worlds.load(repository);
    const auto selected = decideSelectWorld(7, topology);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection().reason, SelectWorldReason::WorldClosed);
    EXPECT_EQ(selected.rejection().worldCount, 2);
}

TEST_F(LoginWorldTopologyTest, ListsActualGroupIDsInAscendingOrderWithNamesAndLiveStatuses) {
    users.getUserInfo(2, 1)->setUserNum(1050);
    const auto listed = serverListFor(1, {}, topology);
    ASSERT_EQ(listed.size(), 2u);
    EXPECT_EQ(listed[0].groupID, 0);
    EXPECT_EQ(listed[0].groupName, "First group");
    EXPECT_EQ(listed[0].stat, SERVER_FREE);
    EXPECT_EQ(listed[1].groupID, 2);
    EXPECT_EQ(listed[1].groupName, repository.groups[1].groupName);
    EXPECT_EQ(listed[1].stat, SERVER_BUSY);
}

TEST_F(LoginWorldTopologyTest, ListedSparseWorldAndGroupKeepTheirActualIDs) {
    const auto selected = decideSelectServer({7, 4}, topology);
    ASSERT_TRUE(selected.isOk());
    EXPECT_EQ(selected.events().worldID, 7);
    EXPECT_EQ(selected.events().serverGroupID, 4);
}

TEST_F(LoginWorldTopologyTest, IDsPastTheEndFallBackToTheHighestConfiguredWorldAndGroup) {
    const auto selected = decideSelectServer({255, 255}, topology);
    ASSERT_TRUE(selected.isOk());
    EXPECT_EQ(selected.events().worldID, 7);
    EXPECT_EQ(selected.events().serverGroupID, 4);
}

TEST_F(LoginWorldTopologyTest, MissingGroupIDsFallBackToTheHighestConfiguredGroup) {
    const auto selected = decideSelectServer({1, 1}, topology);
    ASSERT_TRUE(selected.isOk());
    EXPECT_EQ(selected.events().worldID, 1);
    EXPECT_EQ(selected.events().serverGroupID, 2);
}

TEST_F(LoginWorldTopologyTest, TheGroupCountCannotBecomeAnAbsentGroupID) {
    repository.groups = {{1, 0, "First group", SERVER_FREE}, {1, 1, "Last group", SERVER_FREE}};
    reload();
    const auto selected = decideSelectServer({1, 2}, topology);
    ASSERT_TRUE(selected.isOk());
    EXPECT_EQ(selected.events().serverGroupID, 1);
}

TEST_F(LoginWorldTopologyTest, AnEmptyWorldCatalogueRefusesBothSelectionOperations) {
    repository.worlds.clear();
    worlds.load(repository);
    EXPECT_TRUE(decideSelectWorld(0, topology).isRejected());
    const auto selected = decideSelectServer({0, 9}, topology);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection().reason, SelectServerReason::NoWorlds);
    EXPECT_EQ(selected.rejection().serverGroupID, 9);
}

TEST_F(LoginWorldTopologyTest, AWorldWithoutGroupsListsNothingAndRefusesServerSelection) {
    repository.groups.clear();
    groups.load(repository);
    EXPECT_TRUE(serverListFor(1, {}, topology).empty());
    const auto selected = decideSelectServer({1, 8}, topology);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection().reason, SelectServerReason::NoServerGroups);
    EXPECT_EQ(selected.rejection().serverGroupID, 8);
}

TEST_F(LoginWorldTopologyTest, ZeroAndMaximumIDsRemainSelectableAndListed) {
    repository.worlds = {{255, "Last world", WORLD_OPEN}, {0, "Zero world", WORLD_OPEN}};
    repository.groups = {{255, 255, "Last group", SERVER_FREE}, {0, 0, "Zero group", SERVER_FREE}};
    reload();
    for (const auto id : {0, 255}) {
        EXPECT_TRUE(decideSelectWorld(id, topology).isOk());
        const auto selected =
            decideSelectServer({static_cast<WorldID_t>(id), static_cast<ServerGroupID_t>(id)}, topology);
        ASSERT_TRUE(selected.isOk());
        EXPECT_EQ(selected.events().worldID, id);
        EXPECT_EQ(selected.events().serverGroupID, id);
        const auto listed = serverListFor(id, {}, topology);
        ASSERT_EQ(listed.size(), 1u);
        EXPECT_EQ(listed.front().groupID, id);
    }
}

TEST(LoginWorldTopology, UnloadedManagersHaveEmptyMembershipAndThrowOnlyForDirectRowLookups) {
    GameWorldInfoManager worlds;
    GameServerGroupInfoManager groups;
    UserInfoManager users;
    LoginWorldTopology topology(worlds, groups, users);
    EXPECT_TRUE(topology.worldIDs().empty());
    for (const auto id : {0, 1, 255}) {
        EXPECT_TRUE(topology.serverGroupIDs(id).empty());
        EXPECT_TRUE(serverListFor(id, {}, topology).empty());
        EXPECT_THROW(topology.worldStatus(id), NoSuchElementException);
        EXPECT_THROW(topology.serverGroup(0, id), NoSuchElementException);
        EXPECT_THROW(topology.serverGroupUserNum(0, id), NoSuchElementException);
    }
}

TEST_F(LoginWorldTopologyTest, QuiescentReloadsAreVisibleThroughTheExistingView) {
    repository.worlds = {{9, "Replacement", WORLD_OPEN}};
    repository.groups = {{9, 8, "Replacement group", SERVER_DOWN}};
    reload();
    EXPECT_EQ(topology.worldIDs(), (std::vector<WorldID_t>{9}));
    EXPECT_EQ(topology.serverGroupIDs(9), (std::vector<ServerGroupID_t>{8}));
    EXPECT_TRUE(topology.serverGroupIDs(1).empty());
    EXPECT_TRUE(topology.serverGroupIDs(7).empty());
    EXPECT_TRUE(decideSelectWorld(7, topology).isRejected());
    EXPECT_TRUE(decideSelectWorld(9, topology).isOk());
    const auto selected = decideSelectServer({9, 8}, topology);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection().reason, SelectServerReason::ServerClosed);
    EXPECT_EQ(selected.rejection().serverGroupID, 8);
    users.getUserInfo(8, 9)->setUserNum(101);
    EXPECT_EQ(topology.serverGroupUserNum(8, 9), 101);
}

TEST_F(LoginWorldTopologyTest, MissingWorldIDsInsideTheRangeUseTheHighestConfiguredWorld) {
    for (const auto id : {0, 2, 6}) {
        const auto selected = decideSelectServer({static_cast<WorldID_t>(id), 4}, topology);
        ASSERT_TRUE(selected.isOk());
        EXPECT_EQ(selected.events().worldID, 7);
        EXPECT_EQ(selected.events().serverGroupID, 4);
    }
}

TEST_F(LoginWorldTopologyTest, ADownGroupRemainsListedAndRefusedIncludingAfterNormalization) {
    repository.groups[1].stat = SERVER_DOWN;
    groups.load(repository);
    users.getUserInfo(2, 1)->setUserNum(5000);
    const auto listed = serverListFor(1, {}, topology);
    ASSERT_EQ(listed.size(), 2u);
    EXPECT_EQ(listed[1].stat, SERVER_DOWN);
    for (const auto id : {1, 2, 255}) {
        const auto selected = decideSelectServer({1, static_cast<ServerGroupID_t>(id)}, topology);
        ASSERT_TRUE(selected.isRejected());
        EXPECT_EQ(selected.rejection().reason, SelectServerReason::ServerClosed);
        EXPECT_EQ(selected.rejection().serverGroupID, 2);
    }
}

TEST_F(LoginWorldTopologyTest, ServerSelectionKeepsItsExistingClosedWorldAndPopulationPolicy) {
    repository.worlds[0].stat = WORLD_CLOSE;
    repository.groups[0].stat = 255; // Only SERVER_DOWN refuses a group.
    reload();
    users.getUserInfo(4, 7)->setUserNum(5000);
    EXPECT_TRUE(decideSelectWorld(7, topology).isRejected());
    EXPECT_TRUE(decideSelectServer({7, 4}, topology).isOk());
    const auto listed = serverListFor(7, {}, topology);
    ASSERT_EQ(listed.size(), 1u);
    EXPECT_EQ(listed.front().stat, SERVER_FULL);
}

TEST_F(LoginWorldTopologyTest, MissingPopulationIsAConfigurationErrorOnlyWhenBuildingTheList) {
    UserInfoManager otherUsers;
    LoginWorldTopology otherTopology(worlds, groups, otherUsers);
    EXPECT_TRUE(decideSelectWorld(7, otherTopology).isOk());
    EXPECT_TRUE(decideSelectServer({7, 4}, otherTopology).isOk());
    EXPECT_THROW(serverListFor(7, {}, otherTopology), NoSuchElementException);
    // A failed list build cannot change the supplied metadata or another view's counts.
    users.getUserInfo(4, 7)->setUserNum(1100);
    otherUsers.load(repository);
    const auto listed = serverListFor(7, {}, otherTopology);
    ASSERT_EQ(listed.size(), 1u);
    EXPECT_EQ(listed.front().stat, SERVER_FREE);
    EXPECT_EQ(serverListFor(7, {}, topology).front().stat, SERVER_BUSY);
}

TEST_F(LoginWorldTopologyTest, EnumerationAndSelectionSupportTheEntireStoredIDRange) {
    repository.worlds.clear();
    repository.groups.clear();
    for (int id = 255; id >= 0; --id) {
        repository.worlds.push_back({id, "World", WORLD_OPEN});
        repository.groups.push_back({255, id, "Group", SERVER_FREE});
    }
    reload();
    auto ids = topology.worldIDs();
    std::sort(ids.begin(), ids.end());
    ASSERT_EQ(ids.size(), 256u);
    const auto listed = serverListFor(255, {}, topology);
    ASSERT_EQ(listed.size(), 256u);
    for (int id = 0; id <= 255; ++id) {
        SCOPED_TRACE(id);
        EXPECT_EQ(ids[id], id);
        EXPECT_TRUE(decideSelectWorld(id, topology).isOk());
        EXPECT_EQ(listed[id].groupID, id);
        const auto selected = decideSelectServer({255, static_cast<ServerGroupID_t>(id)}, topology);
        ASSERT_TRUE(selected.isOk());
        EXPECT_EQ(selected.events().worldID, 255);
        EXPECT_EQ(selected.events().serverGroupID, id);
    }
}

} // namespace
