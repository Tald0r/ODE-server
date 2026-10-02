#include <array>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "LCServerList.h"
#include "LoginServerList.h"
#include "Player.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"

namespace {

using Query = de::LoginServerListQuery;
constexpr std::array kQueries{Query::CurrentGroup, Query::CurrentLocation};
enum class FailureStage { Enumeration, Metadata, Population, Account, Send };

class ListTopology : public ServerListTopology {
public:
    struct Group {
        ServerGroupRow row;
        UserNum_t population = 0;
    };

    std::vector<ServerGroupID_t> serverGroupIDs(WorldID_t worldID) override {
        if (failure && failureStage == FailureStage::Enumeration)
            std::rethrow_exception(failure);
        sawExpectedWorld = sawExpectedWorld && worldID == expectedWorld;
        std::vector<ServerGroupID_t> ids;
        for (const auto& group : groups)
            ids.push_back(group.row.groupID);
        return ids;
    }
    ServerGroupRow serverGroup(ServerGroupID_t id, WorldID_t worldID) override {
        if (failure && failureStage == FailureStage::Metadata && id == 2)
            std::rethrow_exception(failure);
        sawExpectedWorld = sawExpectedWorld && worldID == expectedWorld;
        return at(id).row;
    }
    UserNum_t serverGroupUserNum(ServerGroupID_t id, WorldID_t worldID) override {
        if (failure && failureStage == FailureStage::Population && id == 2)
            std::rethrow_exception(failure);
        sawExpectedWorld = sawExpectedWorld && worldID == expectedWorld;
        return at(id).population;
    }
    const Group& at(ServerGroupID_t id) const {
        for (const auto& group : groups)
            if (group.row.groupID == id)
                return group;
        std::abort();
    }

    WorldID_t expectedWorld = 7;
    bool sawExpectedWorld = true;
    FailureStage failureStage = FailureStage::Enumeration;
    std::exception_ptr failure;
    std::vector<Group> groups = {{{2, std::string(96, 'g'), SERVER_FREE}, 1050}, {{0, "", SERVER_DOWN}, 0}};
};

class ListAccounts : public FakeLoginAccountRepository {
public:
    bool loadCurrentServerGroup(const std::string& account, int& group) override {
        ++groupReads;
        sawExpectedAccount = account == expectedAccount;
        if (failure)
            std::rethrow_exception(failure);
        group = savedGroup;
        return found;
    }
    bool loadCurrentLocation(LoginLocationSpelling spelling, const std::string& account, int& world,
                             int& group) override {
        ++locationReads;
        sawExpectedSpelling = spelling == LOGIN_LOCATION_SQL_LOWER;
        sawExpectedAccount = account == expectedAccount;
        if (failure)
            std::rethrow_exception(failure);
        world = savedWorld;
        group = savedGroup;
        return found;
    }

    std::string expectedAccount = "test-account";
    int savedGroup = 2;
    int savedWorld = 7;
    bool found = true;
    unsigned groupReads = 0;
    unsigned locationReads = 0;
    bool sawExpectedAccount = false;
    bool sawExpectedSpelling = false;
    std::exception_ptr failure;
};

class ListPlayer : public Player {
public:
    ListPlayer() {
        setID("test-account");
    }
    void sendPacket(Packet* packet) override {
        auto* reply = dynamic_cast<LCServerList*>(packet);
        if (!reply)
            std::abort();
        ++attempts;
        if (failure)
            std::rethrow_exception(failure);
        currentGroup = reply->getCurrentServerGroupID();
        count = reply->getListNum();
        size = reply->getPacketSize();
        if (capture) {
            rows.clear();
            for (unsigned index = 0; index < count; ++index) {
                std::unique_ptr<ServerGroupInfo> row(reply->popFrontListElement());
                rows.push_back({row->getGroupID(), row->getGroupName(), row->getStat()});
            }
        }
        ++sent;
    }

    bool capture = true;
    unsigned attempts = 0;
    unsigned sent = 0;
    ServerGroupID_t currentGroup = 0;
    unsigned count = 0;
    PacketSize_t size = 0;
    std::vector<ServerGroupRow> rows;
    std::exception_ptr failure;
};

TEST(LoginServerList, BothQueryPoliciesKeepTheirAccountInputsAndSavedGroup) {
    for (const auto query : kQueries) {
        ListTopology topology;
        ListAccounts accounts;
        ListPlayer player;
        de::sendLoginServerList(player, 7, topology, accounts, query);
        EXPECT_EQ(player.sent, 1u);
        EXPECT_EQ(player.currentGroup, 2);
        EXPECT_EQ(accounts.groupReads, query == Query::CurrentGroup ? 1u : 0u);
        EXPECT_EQ(accounts.locationReads, query == Query::CurrentLocation ? 1u : 0u);
        EXPECT_TRUE(accounts.sawExpectedAccount);
        if (query == Query::CurrentLocation)
            EXPECT_TRUE(accounts.sawExpectedSpelling);
        EXPECT_TRUE(topology.sawExpectedWorld);
        EXPECT_EQ(player.getID(), accounts.expectedAccount);
    }
}

TEST(LoginServerList, SparseGroupsAreSortedWithLiveStatusesAndPacketNameTruncation) {
    ListTopology topology;
    ListAccounts accounts;
    ListPlayer player;
    de::sendLoginServerList(player, 7, topology, accounts, Query::CurrentGroup);
    ASSERT_EQ(player.rows.size(), 2u);
    EXPECT_EQ(player.rows[0].groupID, 0);
    EXPECT_TRUE(player.rows[0].groupName.empty());
    EXPECT_EQ(player.rows[0].stat, SERVER_DOWN);
    EXPECT_EQ(player.rows[1].groupID, 2);
    EXPECT_EQ(player.rows[1].groupName, std::string(maxNameLength, 'g'));
    EXPECT_EQ(player.rows[1].stat, SERVER_BUSY);
    EXPECT_EQ(topology.groups[0].row.groupName, std::string(96, 'g'));
}

TEST(LoginServerList, MissingAccountsKeepDefaultZeroEvenWhenOutputArgumentsAreWritten) {
    for (const auto query : kQueries) {
        ListTopology topology;
        ListAccounts accounts;
        accounts.found = false;
        accounts.savedWorld = -1;
        accounts.savedGroup = 256;
        ListPlayer player;
        de::sendLoginServerList(player, 7, topology, accounts, query);
        EXPECT_EQ(player.sent, 1u);
        EXPECT_EQ(player.currentGroup, 0);
    }
}

TEST(LoginServerList, InvalidSavedGroupsCannotNarrowOrSendAReply) {
    for (const auto query : kQueries) {
        for (const int group : {-1, 256, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
            SCOPED_TRACE(group);
            ListTopology topology;
            ListAccounts accounts;
            accounts.savedGroup = group;
            ListPlayer player;
            EXPECT_THROW(de::sendLoginServerList(player, 7, topology, accounts, query), Error);
            EXPECT_EQ(player.attempts, 0u);
        }
    }
}

TEST(LoginServerList, AnOversizedListIsRefusedBeforeAccountLookupOrSendingAndCanRetry) {
    for (const auto query : kQueries) {
        ListTopology topology;
        topology.groups.clear();
        for (unsigned id = 0; id <= ServerGroupInfo::kMaxCount; ++id)
            topology.groups.push_back({{static_cast<ServerGroupID_t>(id), "group", SERVER_FREE}, 0});
        ListAccounts accounts;
        ListPlayer player;
        EXPECT_THROW(de::sendLoginServerList(player, 7, topology, accounts, query), InvalidProtocolException);
        EXPECT_EQ(player.attempts, 0u);
        EXPECT_EQ(accounts.groupReads + accounts.locationReads, 0u);
        topology.groups.pop_back();
        de::sendLoginServerList(player, 7, topology, accounts, query);
        EXPECT_EQ(player.sent, 1u);
        EXPECT_EQ(player.count, ServerGroupInfo::kMaxCount);
    }
}

int checkAllocationFailure(Query query, std::size_t failAt) {
    ListTopology topology;
    ListAccounts accounts;
    accounts.expectedAccount = std::string(96, 'p');
    ListPlayer player;
    player.setID(accounts.expectedAccount);
    player.capture = false;
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        de::sendLoginServerList(player, 7, topology, accounts, query);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || probe.outstanding() != 0 || (failed && player.attempts != 0))
        return 1;
    if (failAt == 64 && failed)
        return 2;
    const auto sent = player.sent;
    de::sendLoginServerList(player, 7, topology, accounts, query);
    return player.sent == sent + 1 && player.count == 2 && player.currentGroup == 2 && probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginServerList, AllocationFailuresReleasePartialRepliesAndPermitRetryForBothQueries) {
    for (const auto query : kQueries) {
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(query, failAt)), ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(LoginServerList, PacketAdderConsumesItsIncomingRowWhenListAllocationFails) {
    ASSERT_EXIT(
        {
            AllocationProbe probe(2); // Record allocation succeeds; list-node allocation fails.
            bool failed = false;
            {
                LCServerList reply;
                try {
                    reply.addListElement(new ServerGroupInfo());
                } catch (const std::bad_alloc&) {
                    failed = true;
                }
            }
            probe.stopFailing();
            std::_Exit(failed && probe.rejected() && probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(LoginServerList, PacketClearReleasesItsRowsAndCanBeReused) {
    ASSERT_EXIT(
        {
            AllocationProbe probe;
            LCServerList reply;
            reply.setCurrentServerGroupID(7);
            for (unsigned repeat = 0; repeat < 3; ++repeat) {
                reply.addListElement(new ServerGroupInfo());
                reply.addListElement(new ServerGroupInfo());
                reply.clearList();
                if (reply.getListNum() != 0 || reply.getCurrentServerGroupID() != 7 || probe.outstanding() != 0)
                    std::_Exit(1);
            }
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(LoginServerList, TheMaximumSparseListFitsTheFactoryBudgetExactly) {
    for (const auto query : kQueries) {
        ListTopology topology;
        topology.groups.clear();
        for (unsigned index = 0; index < ServerGroupInfo::kMaxCount; ++index)
            topology.groups.push_back(
                {{static_cast<ServerGroupID_t>(index * 7), std::string(maxNameLength, 'n'), SERVER_FREE}, 0});
        ListAccounts accounts;
        accounts.savedGroup = 255;
        ListPlayer player;
        de::sendLoginServerList(player, 7, topology, accounts, query);
        EXPECT_EQ(player.sent, 1u);
        EXPECT_EQ(player.currentGroup, 255);
        EXPECT_EQ(player.size, LCServerListFactory::kMaxSize);
        ASSERT_EQ(player.rows.size(), ServerGroupInfo::kMaxCount);
        for (unsigned index = 0; index < player.rows.size(); ++index)
            EXPECT_EQ(player.rows[index].groupID, index * 7);
    }
}

TEST(LoginServerList, EmptyListsKeepSavedBoundaryGroupsWithoutRequiringAMatchingRow) {
    for (const auto query : kQueries) {
        for (const auto id : {0, 1, 255}) {
            ListTopology topology;
            topology.groups.clear();
            ListAccounts accounts;
            accounts.savedGroup = id;
            ListPlayer player;
            de::sendLoginServerList(player, 7, topology, accounts, query);
            EXPECT_EQ(player.sent, 1u);
            EXPECT_EQ(player.currentGroup, id);
            EXPECT_EQ(player.count, 0u);
            EXPECT_EQ(player.size, szServerGroupID + szBYTE);
        }
    }
}

TEST(LoginServerList, OnlyTheSavedGroupFromALocationQueryDeterminesThePacketSelection) {
    for (const int world : {-1, 256, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        ListTopology topology;
        ListAccounts accounts;
        accounts.savedWorld = world;
        accounts.savedGroup = 255;
        ListPlayer player;
        de::sendLoginServerList(player, 7, topology, accounts, Query::CurrentLocation);
        EXPECT_EQ(player.currentGroup, 255);
        EXPECT_TRUE(topology.sawExpectedWorld);
        EXPECT_EQ(player.count, 2u);
    }
}

TEST(LoginServerList, SuppliedThresholdsAndUpdatedPopulationsDetermineEachNewReply) {
    ListTopology topology;
    topology.groups[0].population = 1300;
    ListAccounts accounts;
    ListPlayer player;
    de::sendLoginServerList(player, 7, topology, accounts, Query::CurrentGroup);
    ASSERT_EQ(player.rows.size(), 2u);
    EXPECT_EQ(player.rows[1].stat, SERVER_FULL);
    ServerLoadThresholds thresholds;
    thresholds.veryBusyBelow = 1000;
    thresholds.userMax = 1800;
    de::sendLoginServerList(player, 7, topology, accounts, Query::CurrentGroup, thresholds);
    ASSERT_EQ(player.rows.size(), 2u);
    EXPECT_EQ(player.rows[1].stat, SERVER_VERY_BUSY);
    topology.groups[0].population = 0;
    de::sendLoginServerList(player, 7, topology, accounts, Query::CurrentGroup, thresholds);
    ASSERT_EQ(player.rows.size(), 2u);
    EXPECT_EQ(player.rows[1].stat, SERVER_FREE);
    EXPECT_EQ(player.rows[0].stat, SERVER_DOWN);
}

int checkFailure(Query query, FailureStage stage, unsigned kind) {
    ListTopology topology;
    ListAccounts accounts;
    ListPlayer player;
    player.capture = false;
    const std::array failures{std::make_exception_ptr(std::runtime_error("operation failed")),
                              std::make_exception_ptr(DatabaseError("database failed")),
                              std::make_exception_ptr(Error("catalogue failed")),
                              std::make_exception_ptr(std::bad_alloc{})};
    const auto failure = failures[kind];
    if (stage == FailureStage::Account)
        accounts.failure = failure;
    else if (stage == FailureStage::Send)
        player.failure = failure;
    else {
        topology.failureStage = stage;
        topology.failure = failure;
    }
    AllocationProbe probe;
    std::exception_ptr caught;
    try {
        de::sendLoginServerList(player, 7, topology, accounts, query);
    } catch (...) {
        caught = std::current_exception();
    }
    const auto expectedAttempts = stage == FailureStage::Send ? 1u : 0u;
    if (caught != failure || player.attempts != expectedAttempts || player.sent != 0 || probe.outstanding() != 0)
        return 1;
    const bool beforeAccount =
        stage == FailureStage::Enumeration || stage == FailureStage::Metadata || stage == FailureStage::Population;
    if (accounts.groupReads + accounts.locationReads != (beforeAccount ? 0u : 1u))
        return 2;
    topology.failure = nullptr;
    accounts.failure = nullptr;
    player.failure = nullptr;
    de::sendLoginServerList(player, 7, topology, accounts, query);
    return player.attempts == expectedAttempts + 1 && player.sent == 1 && player.count == 2 && probe.outstanding() == 0
               ? 0
               : 3;
}

TEST(LoginServerList, TopologyAccountAndSendFailuresKeepExceptionIdentityReleaseStorageAndAllowRetry) {
    for (const auto query : kQueries) {
        for (const auto stage : {FailureStage::Enumeration, FailureStage::Metadata, FailureStage::Population,
                                 FailureStage::Account, FailureStage::Send}) {
            for (unsigned kind = 0; kind < 4; ++kind) {
                SCOPED_TRACE(static_cast<int>(query));
                SCOPED_TRACE(static_cast<int>(stage));
                SCOPED_TRACE(kind);
                ASSERT_EXIT(std::_Exit(checkFailure(query, stage, kind)), ::testing::ExitedWithCode(0), "");
            }
        }
    }
}

int checkValidationFailure(Query query) {
    ListTopology topology;
    ListAccounts accounts;
    accounts.savedGroup = 256;
    ListPlayer player;
    player.capture = false;
    AllocationProbe probe;
    bool refused = false;
    try {
        de::sendLoginServerList(player, 7, topology, accounts, query);
    } catch (const Error&) {
        refused = true;
    }
    if (!refused || player.attempts != 0 || probe.outstanding() != 0)
        return 1;
    accounts.savedGroup = 2;
    de::sendLoginServerList(player, 7, topology, accounts, query);
    return player.sent == 1 && player.currentGroup == 2 && probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginServerList, SavedGroupValidationFailureReleasesPreparedDataAndAllowsRetry) {
    for (const auto query : kQueries)
        ASSERT_EXIT(std::_Exit(checkValidationFailure(query)), ::testing::ExitedWithCode(0), "");
}

TEST(LoginServerList, RepeatedPreparationAndSendingReleaseAllTemporaryRows) {
    ASSERT_EXIT(
        {
            ListTopology topology;
            ListAccounts accounts;
            ListPlayer player;
            player.capture = false;
            AllocationProbe probe;
            for (unsigned repeat = 0; repeat < 8; ++repeat) {
                for (const auto query : kQueries) {
                    de::sendLoginServerList(player, 7, topology, accounts, query);
                    if (probe.outstanding() != 0)
                        std::_Exit(1);
                }
            }
            std::_Exit(player.sent == 16 && player.count == 2 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

int checkCountRefusal(std::size_t failAt) {
    LCServerList reply;
    for (unsigned index = 0; index < ServerGroupInfo::kMaxCount; ++index)
        reply.addListElement(new ServerGroupInfo());
    AllocationProbe probe(failAt);
    bool refused = false;
    try {
        reply.addListElement(new ServerGroupInfo());
    } catch (const InvalidProtocolException&) {
        refused = true;
    } catch (const std::bad_alloc&) {
        refused = true;
    }
    probe.stopFailing();
    return refused && reply.getListNum() == ServerGroupInfo::kMaxCount && probe.outstanding() == 0 ? 0 : 1;
}

TEST(LoginServerList, PacketCountRefusalConsumesIncomingOwnershipEvenIfDiagnosticsCannotAllocate) {
    for (std::size_t failAt = 0; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkCountRefusal(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginServerList, PoppingARowTransfersOwnershipAndClearingOnlyReleasesRemainingRows) {
    ASSERT_EXIT(
        {
            AllocationProbe probe;
            {
                LCServerList reply;
                auto first = std::make_unique<ServerGroupInfo>();
                first->setGroupID(7);
                first->setGroupName(std::string(96, 'p'));
                const auto* identity = first.get();
                reply.addListElement(first.release());
                reply.addListElement(new ServerGroupInfo());
                std::unique_ptr<ServerGroupInfo> popped(reply.popFrontListElement());
                reply.clearList();
                if (reply.getListNum() != 0 || popped.get() != identity || popped->getGroupID() != 7 ||
                    popped->getGroupName() != std::string(maxNameLength, 'p'))
                    std::_Exit(1);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

} // namespace
