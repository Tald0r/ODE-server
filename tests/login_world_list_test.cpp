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
#include "GameWorldInfoManager.h"
#include "LCWorldList.h"
#include "LoginWorldList.h"
#include "Player.h"
#include "repository/ServerInfoRepository.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"

namespace {

class WorldRepository : public ServerInfoRepository {
public:
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
    std::vector<ServerInfoWorldRow> loadWorlds() override {
        return rows;
    }

    std::vector<ServerInfoWorldRow> rows = {{2, "Second world", WORLD_CLOSE}, {1, "First world", WORLD_OPEN}};
};

class AccountRepository : public FakeLoginAccountRepository {
public:
    bool loadCurrentWorld(const std::string& account, int& world) override {
        ++reads;
        sawExpectedAccount = account == expectedAccount;
        if (failure)
            std::rethrow_exception(failure);
        world = currentWorld;
        return found;
    }
    void setCurrentLocation(int, int, int, const std::string&) override {
        std::abort();
    }
    void setCurrentServerGroup(int, const std::string&) override {
        std::abort();
    }
    void markLoggedOff(const std::string&) override {
        std::abort();
    }

    std::string expectedAccount = "test-account";
    int currentWorld = 2;
    bool found = true;
    unsigned reads = 0;
    bool sawExpectedAccount = false;
    std::exception_ptr failure;
};

struct ReplyRow {
    WorldID_t id;
    std::string name;
    BYTE status;
};

class RecordingLoginPlayer : public Player {
public:
    RecordingLoginPlayer() {
        setID("test-account");
    }

    void sendPacket(Packet* packet) override {
        auto* reply = dynamic_cast<LCWorldList*>(packet);
        if (!reply)
            std::abort();
        ++attempts;
        if (sendFailure)
            std::rethrow_exception(sendFailure);
        currentWorld = reply->getCurrentWorldID();
        count = reply->getListNum();
        size = reply->getPacketSize();
        if (capture) {
            rows.clear();
            for (unsigned index = 0; index < count; ++index) {
                std::unique_ptr<WorldInfo> row(reply->popFrontListElement());
                rows.push_back({row->getID(), row->getName(), row->getStat()});
            }
        }
        ++sent;
    }

    bool capture = true;
    unsigned attempts = 0;
    unsigned sent = 0;
    WorldID_t currentWorld = 0;
    unsigned count = 0;
    PacketSize_t size = 0;
    std::vector<ReplyRow> rows;
    std::exception_ptr sendFailure;
};

TEST(LoginWorldList, SendsAscendingWorldsAndTheSavedSelectionThroughTheProductionAssembler) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    AccountRepository accounts;
    RecordingLoginPlayer player;
    de::sendLoginWorldList(player, worlds, accounts);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.currentWorld, 2);
    EXPECT_EQ(accounts.reads, 1u);
    EXPECT_TRUE(accounts.sawExpectedAccount);
    ASSERT_EQ(player.rows.size(), 2u);
    EXPECT_EQ(player.rows[0].id, 1);
    EXPECT_EQ(player.rows[0].name, "First world");
    EXPECT_EQ(player.rows[0].status, WORLD_OPEN);
    EXPECT_EQ(player.rows[1].id, 2);
    EXPECT_EQ(player.rows[1].name, "Second world");
    EXPECT_EQ(player.rows[1].status, WORLD_CLOSE);
    EXPECT_EQ(player.getID(), accounts.expectedAccount);
}

TEST(LoginWorldList, MissingAccountKeepsTheExistingDefaultWorldEvenIfTheOutputArgumentWasWritten) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    AccountRepository accounts;
    accounts.found = false;
    accounts.currentWorld = -1;
    RecordingLoginPlayer player;
    de::sendLoginWorldList(player, worlds, accounts);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.currentWorld, 1);
    EXPECT_EQ(player.count, 2u);
}

TEST(LoginWorldList, EmptyCatalogueStillSendsTheSavedWorldWithoutRequiringAMatchingEntry) {
    GameWorldInfoManager worlds;
    AccountRepository accounts;
    accounts.currentWorld = 255;
    RecordingLoginPlayer player;
    de::sendLoginWorldList(player, worlds, accounts);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.currentWorld, 255);
    EXPECT_EQ(player.count, 0u);
}

TEST(LoginWorldList, SparseWorldsIncludingZeroAndTheMaximumIDAreSentInAscendingOrder) {
    WorldRepository repository;
    repository.rows = {{255, "Last", WORLD_CLOSE}, {7, "Middle", WORLD_OPEN}, {0, "Zero", WORLD_OPEN}};
    GameWorldInfoManager worlds;
    worlds.load(repository);
    AccountRepository accounts;
    accounts.currentWorld = 7;
    RecordingLoginPlayer player;
    ASSERT_NO_THROW(de::sendLoginWorldList(player, worlds, accounts));
    ASSERT_EQ(player.rows.size(), 3u);
    EXPECT_EQ(player.rows[0].id, 0);
    EXPECT_EQ(player.rows[1].id, 7);
    EXPECT_EQ(player.rows[2].id, 255);
    EXPECT_EQ(player.currentWorld, 7);
    EXPECT_EQ(player.rows[2].status, WORLD_CLOSE);
}

TEST(LoginWorldList, InvalidSavedWorldIDsCannotNarrowOrSendAReply) {
    for (const int id : {-1, 256, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        SCOPED_TRACE(id);
        GameWorldInfoManager worlds;
        AccountRepository accounts;
        accounts.currentWorld = id;
        RecordingLoginPlayer player;
        EXPECT_THROW(de::sendLoginWorldList(player, worlds, accounts), Error);
        EXPECT_EQ(player.attempts, 0u);
    }
}

int checkLookupFailure(unsigned kind) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    AccountRepository accounts;
    const std::array failures{std::make_exception_ptr(std::runtime_error("lookup failed")),
                              std::make_exception_ptr(DatabaseError("database failed")),
                              std::make_exception_ptr(Error("world failed"))};
    accounts.failure = failures[kind];
    RecordingLoginPlayer player;
    player.capture = false;
    const auto* previous = worlds.getGameWorldInfo(1);
    AllocationProbe probe;
    std::exception_ptr caught;
    try {
        de::sendLoginWorldList(player, worlds, accounts);
    } catch (...) {
        caught = std::current_exception();
    }
    if (caught != accounts.failure || player.attempts != 0 || worlds.getGameWorldInfo(1) != previous)
        return 1;
    if (probe.outstanding() != 0)
        return 2;
    accounts.failure = nullptr;
    de::sendLoginWorldList(player, worlds, accounts);
    return player.sent == 1 && player.count == 2 && probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginWorldList, RepositoryFailureReleasesPreparedRowsPreservesExceptionIdentityAndAllowsRetry) {
    for (unsigned kind = 0; kind < 3; ++kind) {
        SCOPED_TRACE(kind);
        ASSERT_EXIT(std::_Exit(checkLookupFailure(kind)), ::testing::ExitedWithCode(0), "");
    }
}

int checkAllocationFailure(std::size_t failAt) {
    WorldRepository repository;
    repository.rows[0].name = std::string(96, 'a');
    repository.rows[1].name = std::string(128, 'b');
    GameWorldInfoManager worlds;
    worlds.load(repository);
    const auto* previous = worlds.getGameWorldInfo(1);
    AccountRepository accounts;
    accounts.expectedAccount = std::string(96, 'p');
    RecordingLoginPlayer player;
    player.setID(accounts.expectedAccount);
    player.capture = false;
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        de::sendLoginWorldList(player, worlds, accounts);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 128 && failed))
        return 1;
    if (worlds.getGameWorldInfo(1) != previous || previous->getName() != repository.rows[1].name)
        return 2;
    if (probe.outstanding() != 0)
        return 3;
    if (failed) {
        if (player.attempts != 0)
            return 4;
        de::sendLoginWorldList(player, worlds, accounts);
    }
    return player.sent == 1 && player.count == 2 && player.currentWorld == 2 && accounts.sawExpectedAccount &&
                   probe.outstanding() == 0
               ? 0
               : 5;
}

TEST(LoginWorldList, AllocationFailuresReleaseRowsAvoidSendingAndPermitRetry) {
    for (std::size_t failAt = 1; failAt <= 128; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginWorldList, PacketAdderConsumesTheIncomingRowWhenListAllocationFails) {
    ASSERT_EXIT(
        {
            AllocationProbe probe(2);
            bool failed = false;
            {
                LCWorldList reply;
                try {
                    reply.addListElement(new WorldInfo());
                } catch (const std::bad_alloc&) {
                    failed = true;
                }
            }
            probe.stopFailing();
            std::_Exit(failed && probe.rejected() && probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(LoginWorldList, PacketClearReleasesItsOwnedRowsAndCanBeReused) {
    ASSERT_EXIT(
        {
            AllocationProbe probe;
            LCWorldList reply;
            reply.setCurrentWorldID(7);
            for (unsigned repeat = 0; repeat < 3; ++repeat) {
                reply.addListElement(new WorldInfo());
                reply.addListElement(new WorldInfo());
                reply.clearList();
                if (reply.getListNum() != 0 || reply.getCurrentWorldID() != 7 || probe.outstanding() != 0)
                    std::_Exit(1);
            }
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(LoginWorldList, ReplyNamesKeepTruncationAndEmptyTextWithoutChangingTheCatalogue) {
    WorldRepository repository;
    repository.rows = {{1, "", WORLD_OPEN}, {2, std::string(96, 'w'), WORLD_CLOSE}};
    GameWorldInfoManager worlds;
    worlds.load(repository);
    AccountRepository accounts;
    RecordingLoginPlayer player;
    de::sendLoginWorldList(player, worlds, accounts);
    ASSERT_EQ(player.rows.size(), 2u);
    EXPECT_TRUE(player.rows[0].name.empty());
    EXPECT_EQ(player.rows[1].name, std::string(maxNameLength, 'w'));
    EXPECT_EQ(worlds.getGameWorldInfo(2)->getName(), repository.rows[1].name);
}

TEST(LoginWorldList, AFullListOfSparseWorldsFitsTheFactoryBudgetExactly) {
    WorldRepository repository;
    repository.rows.clear();
    for (unsigned index = 0; index < WorldInfo::kMaxCount; ++index)
        repository.rows.push_back({static_cast<int>(index * 7), std::string(maxNameLength, 'w'), WORLD_OPEN});
    GameWorldInfoManager worlds;
    worlds.load(repository);
    AccountRepository accounts;
    accounts.currentWorld = 0;
    RecordingLoginPlayer player;
    de::sendLoginWorldList(player, worlds, accounts);
    ASSERT_EQ(player.rows.size(), WorldInfo::kMaxCount);
    EXPECT_EQ(player.size, LCWorldListFactory::kMaxSize);
    EXPECT_EQ(player.currentWorld, 0);
    for (unsigned index = 0; index < player.rows.size(); ++index)
        EXPECT_EQ(player.rows[index].id, index * 7);
}

TEST(LoginWorldList, OversizedCataloguesAreRefusedBeforeAccountLookupOrSending) {
    WorldRepository repository;
    repository.rows.clear();
    for (unsigned index = 0; index <= WorldInfo::kMaxCount; ++index)
        repository.rows.push_back({static_cast<int>(index), "World", WORLD_OPEN});
    GameWorldInfoManager worlds;
    worlds.load(repository);
    AccountRepository accounts;
    RecordingLoginPlayer player;
    EXPECT_THROW(de::sendLoginWorldList(player, worlds, accounts), InvalidProtocolException);
    EXPECT_EQ(player.attempts, 0u);
    EXPECT_EQ(accounts.reads, 0u);
    EXPECT_EQ(worlds.getSize(), WorldInfo::kMaxCount + 1);
    repository.rows.pop_back();
    worlds.load(repository);
    de::sendLoginWorldList(player, worlds, accounts);
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.count, WorldInfo::kMaxCount);
}

TEST(LoginWorldList, SavedWorldBoundariesDoNotRequireAMatchingCatalogueEntry) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    for (const int id : {0, 1, 255}) {
        SCOPED_TRACE(id);
        AccountRepository accounts;
        accounts.currentWorld = id;
        RecordingLoginPlayer player;
        de::sendLoginWorldList(player, worlds, accounts);
        EXPECT_EQ(player.sent, 1u);
        EXPECT_EQ(player.currentWorld, id);
        EXPECT_EQ(player.count, 2u);
    }
}

int checkSendFailure(unsigned kind) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    AccountRepository accounts;
    RecordingLoginPlayer player;
    player.capture = false;
    const std::array failures{std::make_exception_ptr(std::runtime_error("send failed")),
                              std::make_exception_ptr(Error("send failed")), std::make_exception_ptr(std::bad_alloc{})};
    player.sendFailure = failures[kind];
    const auto* previous = worlds.getGameWorldInfo(1);
    AllocationProbe probe;
    std::exception_ptr caught;
    try {
        de::sendLoginWorldList(player, worlds, accounts);
    } catch (...) {
        caught = std::current_exception();
    }
    if (caught != player.sendFailure || player.attempts != 1 || player.sent != 0 ||
        worlds.getGameWorldInfo(1) != previous || probe.outstanding() != 0)
        return 1;
    player.sendFailure = nullptr;
    de::sendLoginWorldList(player, worlds, accounts);
    return player.attempts == 2 && player.sent == 1 && player.count == 2 && probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginWorldList, ThrowingSendReleasesTheCompletedReplyPreservesExceptionIdentityAndAllowsRetry) {
    for (unsigned kind = 0; kind < 3; ++kind) {
        SCOPED_TRACE(kind);
        ASSERT_EXIT(std::_Exit(checkSendFailure(kind)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginWorldList, RepeatedPreparationAndSendingReleaseAllTemporaryRows) {
    ASSERT_EXIT(
        {
            WorldRepository repository;
            GameWorldInfoManager worlds;
            worlds.load(repository);
            AccountRepository accounts;
            RecordingLoginPlayer player;
            player.capture = false;
            AllocationProbe probe;
            for (unsigned repeat = 0; repeat < 8; ++repeat) {
                de::sendLoginWorldList(player, worlds, accounts);
                if (probe.outstanding() != 0)
                    std::_Exit(1);
            }
            std::_Exit(player.sent == 8 && player.count == 2 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

int checkCountRefusalAllocation(std::size_t failAt) {
    LCWorldList reply;
    for (unsigned index = 0; index < WorldInfo::kMaxCount; ++index)
        reply.addListElement(new WorldInfo());
    AllocationProbe probe(failAt);
    bool refused = false;
    try {
        reply.addListElement(new WorldInfo());
    } catch (const InvalidProtocolException&) {
        refused = true;
    } catch (const std::bad_alloc&) {
        refused = true;
    }
    probe.stopFailing();
    return refused && reply.getListNum() == WorldInfo::kMaxCount && probe.outstanding() == 0 ? 0 : 1;
}

TEST(LoginWorldList, PacketCountRefusalConsumesTheIncomingRowEvenIfDiagnosticAllocationFails) {
    for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkCountRefusalAllocation(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginWorldList, ValidationFailureAfterPreparingRowsReleasesTheReplyAndAllowsRetry) {
    ASSERT_EXIT(
        {
            WorldRepository repository;
            GameWorldInfoManager worlds;
            worlds.load(repository);
            AccountRepository accounts;
            accounts.currentWorld = 256;
            RecordingLoginPlayer player;
            player.capture = false;
            AllocationProbe probe;
            bool refused = false;
            try {
                de::sendLoginWorldList(player, worlds, accounts);
            } catch (const Error&) {
                refused = true;
            }
            if (!refused || player.attempts != 0 || probe.outstanding() != 0)
                std::_Exit(1);
            accounts.currentWorld = 2;
            de::sendLoginWorldList(player, worlds, accounts);
            std::_Exit(player.sent == 1 && player.count == 2 && probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

} // namespace
