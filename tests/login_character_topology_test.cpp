#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "LoginCharacterTopology.h"
#include "repository/LoginConfigRepository.h"
#include "repository/ServerInfoRepository.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

class TopologyRepository : public ServerInfoRepository, public LoginConfigRepository {
public:
    bool loadMaxServerGroupID(int& maximum) override {
        maximum = maxGroup;
        return true;
    }
    bool loadMaxWorldID(int& maximum) override {
        maximum = maxWorld;
        return true;
    }
    std::vector<ServerInfoRow> loadServers() override {
        return servers;
    }
    std::vector<ServerInfoNonPKRow> loadNonPKServers() override {
        return nonPK;
    }
    std::vector<ServerInfoCastleStatRow> loadCastleStats() override {
        return {};
    }
    std::vector<ServerInfoWorldRow> loadWorlds() override {
        std::abort();
    }
    std::vector<LoginZoneRow> loadZones() override {
        return zones;
    }
    std::vector<LoginZoneGroupRow> loadZoneGroups() override {
        return groups;
    }
    bool loadMaxGameServerGroupWorldID(int&) override {
        std::abort();
    }
    std::vector<LoginGameServerGroupRow> loadGameServerGroups() override {
        std::abort();
    }
    std::vector<LoginGameServerGroupIDRow> loadGameServerGroupIDs() override {
        std::abort();
    }
    bool loadClientVersion(int&) override {
        std::abort();
    }

    int maxWorld = 7;
    int maxGroup = 9;
    std::vector<ServerInfoRow> servers = {{1, "Normal", "192.0.2.1", 9991, 9992, 1, 0, SERVER_FREE},
                                          {1, "Non-PK", "192.0.2.2", 9993, 9994, 7, 9, SERVER_FREE},
                                          {65535, "Destination", "192.0.2.3", 9995, 9996, 7, 9, SERVER_FREE},
                                          {1, "Zero first", "192.0.2.4", 9997, 9998, 0, 0, SERVER_FREE},
                                          {0, "Zero destination", "192.0.2.5", 9999, 10000, 0, 0, SERVER_FREE}};
    std::vector<ServerInfoNonPKRow> nonPK = {{7, 9}, {0, 0}};
    std::vector<LoginZoneRow> zones = {{100, 300}, {40000, 60000}, {0, 0}, {65535, 65535}};
    std::vector<LoginZoneGroupRow> groups = {{300, 10}, {60000, 65535}, {0, 0}, {65535, 65535}};
};

class Characters : public FakeLoginCharacterRepository {
public:
    bool loadCharacterForSelect(WorldID_t world, LoginRaceTable table, const std::string& name,
                                const std::string& account, LoginSelectRow& row) override {
        lastWorld = world;
        if (failure)
            std::rethrow_exception(failure);
        return FakeLoginCharacterRepository::loadCharacterForSelect(world, table, name, account, row);
    }
    WorldID_t lastWorld = 0;
    std::exception_ptr failure;
};

SelectPCRequest request() {
    SelectPCRequest result;
    result.worldID = 7;
    result.serverGroupID = 9;
    result.playerID = "account";
    result.pcName = "Rowan";
    result.inCharacterManagement = true;
    return result;
}

class LoginCharacterTopologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        servers.load(repository);
        zones.load(repository);
        groups.load(repository);
    }
    TopologyRepository repository;
    GameServerInfoManager servers;
    ZoneInfoManager zones;
    ZoneGroupInfoManager groups;
    LoginCharacterTopology topology{servers, zones, groups};
};

TEST_F(LoginCharacterTopologyTest, ReadsTheRequestedWorldAndGroupFromTheSuppliedServerCatalogue) {
    EXPECT_FALSE(topology.isNonPKServer(1, 0));
    EXPECT_TRUE(topology.isNonPKServer(7, 9));
    EXPECT_TRUE(topology.isNonPKServer(0, 0));
}

TEST_F(LoginCharacterTopologyTest, RoutesSparseZoneAndGroupIDsWithoutNarrowingWordSizedServerIDs) {
    EXPECT_EQ(topology.zoneServerID(100), 10);
    EXPECT_EQ(topology.zoneServerID(40000), 65535);
    EXPECT_EQ(topology.zoneServerID(0), 0);
    EXPECT_EQ(topology.zoneServerID(65535), 65535);
}

TEST_F(LoginCharacterTopologyTest, MissingWorldGroupAndFirstServerRemainConfigurationErrors) {
    EXPECT_THROW(topology.isNonPKServer(2, 0), NoSuchElementException);
    EXPECT_THROW(topology.isNonPKServer(7, 8), NoSuchElementException);
    repository.nonPK.clear();
    std::erase_if(repository.servers, [](const auto& row) { return row.worldID == 7 && row.serverID == 1; });
    servers.load(repository);
    EXPECT_THROW(topology.isNonPKServer(7, 9), NoSuchElementException);
}

TEST_F(LoginCharacterTopologyTest, MissingZonesAndZoneGroupsDoNotProduceAFallbackServer) {
    EXPECT_THROW(topology.zoneServerID(101), NoSuchElementException);
    std::erase_if(repository.groups, [](const auto& row) { return row.zoneGroupID == 300; });
    groups.load(repository);
    EXPECT_THROW(topology.zoneServerID(100), NoSuchElementException);
    EXPECT_EQ(topology.zoneServerID(40000), 65535);
}

TEST_F(LoginCharacterTopologyTest, EachLookupReadsOnlyItsOwnDependencies) {
    GameServerInfoManager emptyServers;
    LoginCharacterTopology routingOnly(emptyServers, zones, groups);
    EXPECT_EQ(routingOnly.zoneServerID(40000), 65535);
    EXPECT_THROW(routingOnly.isNonPKServer(7, 9), NoSuchElementException);
    ZoneInfoManager emptyZones;
    ZoneGroupInfoManager emptyGroups;
    LoginCharacterTopology flagsOnly(servers, emptyZones, emptyGroups);
    EXPECT_TRUE(flagsOnly.isNonPKServer(7, 9));
    EXPECT_THROW(flagsOnly.zoneServerID(40000), NoSuchElementException);
}

TEST_F(LoginCharacterTopologyTest, QuiescentReloadsAreVisibleWithoutRebuildingTheAdapter) {
    repository.nonPK.clear();
    repository.zones = {{100, 50000}};
    repository.groups = {{50000, 54321}};
    servers.load(repository);
    zones.load(repository);
    groups.load(repository);
    EXPECT_FALSE(topology.isNonPKServer(7, 9));
    EXPECT_EQ(topology.zoneServerID(100), 54321);
    EXPECT_THROW(topology.zoneServerID(40000), NoSuchElementException);
}

TEST_F(LoginCharacterTopologyTest, IndependentViewsKeepTheirOwnCatalogues) {
    TopologyRepository otherRepository;
    otherRepository.nonPK.clear();
    otherRepository.groups = {{300, 44}};
    GameServerInfoManager otherServers;
    ZoneInfoManager otherZones;
    ZoneGroupInfoManager otherGroups;
    otherServers.load(otherRepository);
    otherZones.load(otherRepository);
    otherGroups.load(otherRepository);
    LoginCharacterTopology other(otherServers, otherZones, otherGroups);
    EXPECT_FALSE(other.isNonPKServer(7, 9));
    EXPECT_EQ(other.zoneServerID(100), 44);
    EXPECT_TRUE(topology.isNonPKServer(7, 9));
    EXPECT_EQ(topology.zoneServerID(100), 10);
}

TEST_F(LoginCharacterTopologyTest, RealCatalogueRoutingFeedsAcceptedSelectionsForEveryRace) {
    for (const auto race : {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS}) {
        Characters characters;
        auto selectedRequest = request();
        selectedRequest.pcType = race;
        const auto table = race == PC_SLAYER    ? LOGIN_RACE_TABLE_SLAYER
                           : race == PC_VAMPIRE ? LOGIN_RACE_TABLE_VAMPIRE
                                                : LOGIN_RACE_TABLE_OUSTERS;
        characters.addSelectableCharacter(table, "account", "Rowan", 40000, "SLOT2", 30, 1);
        const auto selected = decideSelectPC(selectedRequest, characters, topology);
        ASSERT_TRUE(selected.isOk());
        EXPECT_EQ(selected.events().table, table);
        EXPECT_EQ(selected.events().zoneID, 40000);
        EXPECT_EQ(selected.events().serverID, 65535);
        EXPECT_EQ(selected.events().slot, 2);
        EXPECT_EQ(characters.lastWorld, 7);
    }
}

TEST_F(LoginCharacterTopologyTest, NonPKRefusalPrecedesAMissingZoneAndKeepsTheLevelBoundary) {
    Characters characters;
    characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 9999, "SLOT2", 81, 3);
    auto selected = decideSelectPC(request(), characters, topology);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection(), SelectPCRejection::NonPKServerLimit);
    characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 40000, "SLOT2", 80, 3);
    EXPECT_TRUE(decideSelectPC(request(), characters, topology).isOk());
    repository.nonPK.clear();
    servers.load(repository);
    characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 9999, "SLOT2", 81, 3);
    EXPECT_THROW((void)decideSelectPC(request(), characters, topology), NoSuchElementException);
}

TEST_F(LoginCharacterTopologyTest, QuestInteriorsSkipRoutingButTheirBoundaryZonesStillNeedRows) {
    ZoneInfoManager emptyZones;
    ZoneGroupInfoManager emptyGroups;
    LoginCharacterTopology questTopology(servers, emptyZones, emptyGroups);
    Characters characters;
    for (const auto zone : {10001, 29999}) {
        characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", zone, "SLOT1", 30, 1);
        const auto selected = decideSelectPC(request(), characters, questTopology);
        ASSERT_TRUE(selected.isOk());
        EXPECT_EQ(selected.events().serverID, 1);
        EXPECT_EQ(selected.events().zoneID, zone);
    }
    for (const auto zone : {10000, 30000}) {
        characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", zone, "SLOT1", 30, 1);
        EXPECT_THROW((void)decideSelectPC(request(), characters, questTopology), NoSuchElementException);
    }
}

TEST_F(LoginCharacterTopologyTest, EarlyInputAndCharacterGatesDoNotRequireLoadedCatalogues) {
    GameServerInfoManager emptyServers;
    ZoneInfoManager emptyZones;
    ZoneGroupInfoManager emptyGroups;
    LoginCharacterTopology empty(emptyServers, emptyZones, emptyGroups);
    Characters characters;
    auto input = request();
    input.agreedToTerms = false;
    auto selected = decideSelectPC(input, characters, empty);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection(), SelectPCRejection::DidNotAgree);
    input.agreedToTerms = true;
    input.inCharacterManagement = false;
    selected = decideSelectPC(input, characters, empty);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection(), SelectPCRejection::InvalidStatus);
    EXPECT_EQ(characters.loadCharacterForSelectCalls, 0);
    input.inCharacterManagement = true;
    selected = decideSelectPC(input, characters, empty);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection(), SelectPCRejection::NoSuchCharacter);
    characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 100, "SLOT1", 81, 3);
    input.checkFreePlayLimit = true;
    input.freePlaySlayerDomainSum = 80;
    selected = decideSelectPC(input, characters, empty);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection(), SelectPCRejection::FreePlayLimit);
}

TEST_F(LoginCharacterTopologyTest, ExistingSlotLengthRefusalPrecedesZoneRouting) {
    Characters characters;
    characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 9999, "SLOT", 30, 1);
    const auto selected = decideSelectPC(request(), characters, topology);
    ASSERT_TRUE(selected.isRejected());
    EXPECT_EQ(selected.rejection(), SelectPCRejection::NoSlot);
}

TEST_F(LoginCharacterTopologyTest, MissingCatalogueReferencesPropagateThroughTheDecision) {
    Characters characters;
    characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 40000, "SLOT1", 30, 1);
    auto input = request();
    input.worldID = 6;
    EXPECT_THROW((void)decideSelectPC(input, characters, topology), NoSuchElementException);
    input = request();
    repository.groups.clear();
    groups.load(repository);
    EXPECT_THROW((void)decideSelectPC(input, characters, topology), NoSuchElementException);
}

TEST_F(LoginCharacterTopologyTest, RepositoryFailuresKeepTheirIdentityBeforeAnyCatalogueLookup) {
    GameServerInfoManager emptyServers;
    LoginCharacterTopology empty(emptyServers, zones, groups);
    Characters characters;
    const std::array failures{std::make_exception_ptr(std::runtime_error("lookup failed")),
                              std::make_exception_ptr(DatabaseError("lookup failed")),
                              std::make_exception_ptr(Error("lookup failed"))};
    for (const auto& failure : failures) {
        characters.failure = failure;
        std::exception_ptr caught;
        try {
            (void)decideSelectPC(request(), characters, empty);
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
    }
}

TEST_F(LoginCharacterTopologyTest, NonPKLookupSupportsTheZeroAndMaximumWorldAndGroupIDs) {
    repository.maxWorld = repository.maxGroup = 255;
    repository.servers.push_back({1, "Last", "192.0.2.255", 9991, 9992, 255, 255, SERVER_FREE});
    repository.nonPK.push_back({255, 255});
    servers.load(repository);
    EXPECT_TRUE(topology.isNonPKServer(0, 0));
    EXPECT_TRUE(topology.isNonPKServer(255, 255));
    EXPECT_THROW(topology.isNonPKServer(255, 254), NoSuchElementException);
}

} // namespace
