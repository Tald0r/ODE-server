#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <string_view>
#include <type_traits>

#include "DatabaseError.h"
#include "GameServerInfoManager.h"
#include "repository/ServerInfoRepository.h"
#include "support/AllocationProbe.h"

namespace {

class ServerRepository : public ServerInfoRepository {
public:
    bool loadMaxServerGroupID(int& value) override {
        visit(0);
        value = maxGroup;
        return hasGroup;
    }
    bool loadMaxWorldID(int& value) override {
        visit(1);
        value = maxWorld;
        return hasWorld;
    }
    std::vector<ServerInfoRow> loadServers() override {
        visit(2);
        return servers;
    }
    std::vector<ServerInfoNonPKRow> loadNonPKServers() override {
        visit(3);
        return nonPK;
    }
    std::vector<ServerInfoCastleStatRow> loadCastleStats() override {
        visit(4);
        return castles;
    }
    std::vector<ServerInfoWorldRow> loadWorlds() override {
        std::abort();
    }
    void visit(unsigned stage) {
        visits[stage] = ++reads;
        if (failure && stage == failureStage)
            std::rethrow_exception(failure);
    }

    int maxGroup = 2;
    int maxWorld = 2;
    bool hasGroup = true;
    bool hasWorld = true;
    unsigned reads = 0;
    std::array<unsigned, 5> visits{};
    unsigned failureStage = 0;
    std::exception_ptr failure;
    std::vector<ServerInfoRow> servers = {{1, "First server", "127.0.0.1", 9991, 9992, 1, 0, SERVER_FREE},
                                          {7, "Another server", "192.0.2.7", 9993, 9994, 1, 0, SERVER_DOWN},
                                          {1, "Other world", "192.0.2.1", 9995, 9996, 2, 1, SERVER_BUSY}};
    std::vector<ServerInfoNonPKRow> nonPK = {{1, 0}};
    std::vector<ServerInfoCastleStatRow> castles = {{1, 0, 12}, {2, 1, 23}};
};

ServerRepository replacement() {
    ServerRepository repository;
    repository.maxGroup = 3;
    repository.maxWorld = 3;
    repository.servers = {{1, std::string(96, 'a'), std::string(96, 'i'), 9991, 9992, 3, 3, SERVER_NORMAL}};
    repository.nonPK = {{3, 3}};
    repository.castles = {{3, 3, 45}};
    return repository;
}

bool retained(GameServerInfoManager& manager, const GameServerInfo* previous, HashMapGameServerInfo** view) {
    return manager.getMaxWorldID() == 4 && manager.getMaxServerGroupID() == 3 && manager.getGameServerInfos() == view &&
           manager.getGameServerInfo(1, 0, 1) == previous && previous->getCastleFollowingServerID() == 12 &&
           previous->isNonPKServer();
}

void checkColdLifetime() {
    alignas(GameServerInfoManager) unsigned char storage[sizeof(GameServerInfoManager)];
    std::fill_n(storage, sizeof(storage), 0xA5);
    auto* manager = ::new (storage) GameServerInfoManager;
    std::destroy_at(manager);
    std::_Exit(0);
}

TEST(ServerCatalogue, AnUnloadedManagerHasASafeDestructor) {
    ASSERT_EXIT(checkColdLifetime(), ::testing::ExitedWithCode(0), "");
}

TEST(ServerCatalogue, AnAbsentCastleOverlayHasADefinedZeroDefault) {
    alignas(GameServerInfo) unsigned char storage[sizeof(GameServerInfo)];
    std::fill_n(storage, sizeof(storage), 0xA5);
    auto* info = ::new (storage) GameServerInfo;
    EXPECT_FALSE(info->isNonPKServer());
    EXPECT_EQ(info->getCastleFollowingServerID(), 0);
    std::destroy_at(info);
}

TEST(ServerCatalogue, LoadsSuppliedFieldsFlagsAndTheExistingTraversalDimensions) {
    ServerRepository repository;
    GameServerInfoManager manager;
    manager.load(repository);
    EXPECT_EQ(repository.visits, (std::array<unsigned, 5>{1, 2, 3, 4, 5}));
    EXPECT_EQ(manager.getMaxWorldID(), 4);
    EXPECT_EQ(manager.getMaxServerGroupID(), 3);
    const auto* first = manager.getGameServerInfo(1, 0, 1);
    EXPECT_EQ(first->getServerID(), 1);
    EXPECT_EQ(first->getWorldID(), 1);
    EXPECT_EQ(first->getGroupID(), 0);
    EXPECT_EQ(first->getNickname(), "First server");
    EXPECT_EQ(first->getIP(), "127.0.0.1");
    EXPECT_EQ(first->getTCPPort(), 9991u);
    EXPECT_EQ(first->getUDPPort(), 9992u);
    EXPECT_EQ(first->getServerStat(), SERVER_FREE);
    EXPECT_TRUE(first->isNonPKServer());
    EXPECT_EQ(first->getCastleFollowingServerID(), 12);
    EXPECT_FALSE(manager.getGameServerInfo(7, 0, 1)->isNonPKServer());
    EXPECT_EQ(manager.getGameServerInfo(1, 1, 2)->getCastleFollowingServerID(), 23);
    EXPECT_EQ(manager.getGameServerInfos()[1][0].at(1), first);
    EXPECT_EQ(manager.getSize(1, 0), 2u);
}

TEST(ServerCatalogue, CastleFollowingIDsKeepTheirStoredWordWidth) {
    ServerRepository repository;
    repository.castles = {{1, 0, 65535}, {2, 1, 256}};
    GameServerInfoManager manager;
    manager.load(repository);
    EXPECT_EQ(manager.getGameServerInfo(1, 0, 1)->getCastleFollowingServerID(), 65535);
    EXPECT_EQ(manager.getGameServerInfo(1, 1, 2)->getCastleFollowingServerID(), 256);
}

int checkWorldZero() {
    ServerRepository repository;
    repository.maxWorld = 0;
    repository.maxGroup = 0;
    repository.servers = {{1, "Zero world", "127.0.0.1", 9991, 9992, 0, 0, SERVER_FREE}};
    repository.nonPK = {{0, 0}};
    repository.castles = {{0, 0, 1}};
    AllocationProbe probe;
    {
        GameServerInfoManager manager;
        manager.load(repository);
        if (manager.getMaxWorldID() != 2 || manager.getSize(0, 0) != 1 ||
            !manager.getGameServerInfo(1, 0, 0)->isNonPKServer())
            return 1;
    }
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(ServerCatalogue, WorldZeroHasOwnedStorageAndSupportsBothFlags) {
    ASSERT_EXIT(std::_Exit(checkWorldZero()), ::testing::ExitedWithCode(0), "");
}

int checkFetchFailure(unsigned stage) {
    ServerRepository original;
    auto next = replacement();
    GameServerInfoManager manager;
    manager.load(original);
    const auto* previous = manager.getGameServerInfo(1, 0, 1);
    auto** view = manager.getGameServerInfos();
    next.failureStage = stage;
    next.failure = std::make_exception_ptr(std::runtime_error("query failed"));
    std::exception_ptr caught;
    try {
        manager.load(next);
    } catch (...) {
        caught = std::current_exception();
    }
    return caught == next.failure && retained(manager, previous, view) ? 0 : 1;
}

TEST(ServerCatalogue, FailureAtEveryRepositoryStagePreservesThePreviousRowsAndTraversal) {
    for (unsigned stage = 0; stage < 5; ++stage) {
        SCOPED_TRACE(stage);
        ASSERT_EXIT(std::_Exit(checkFetchFailure(stage)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(ServerCatalogue, DuplicateBaseRowsCannotPublishAPartialReplacement) {
    ServerRepository original;
    auto next = replacement();
    next.servers.push_back(next.servers.front());
    GameServerInfoManager manager;
    manager.load(original);
    const auto* previous = manager.getGameServerInfo(1, 0, 1);
    auto** view = manager.getGameServerInfos();
    EXPECT_THROW(manager.load(next), DuplicatedException);
    EXPECT_TRUE(retained(manager, previous, view));
}

TEST(ServerCatalogue, MissingOverlayTargetsCannotPublishTheBaseRows) {
    for (const bool castle : {false, true}) {
        ServerRepository original;
        auto next = replacement();
        if (castle)
            next.castles.push_back({1, 0, 3});
        else
            next.nonPK.push_back({1, 0});
        GameServerInfoManager manager;
        manager.load(original);
        const auto* previous = manager.getGameServerInfo(1, 0, 1);
        auto** view = manager.getGameServerInfos();
        EXPECT_THROW(manager.load(next), NoSuchElementException);
        EXPECT_TRUE(retained(manager, previous, view));
    }
}

int checkReloadCleanup() {
    ServerRepository original;
    auto next = replacement();
    AllocationProbe probe;
    {
        GameServerInfoManager manager;
        manager.load(original);
        manager.load(next);
        if (manager.getSize(1, 0) != 0 || manager.getSize(3, 3) != 1)
            return 1;
    }
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(ServerCatalogue, SuccessfulReloadReleasesThePreviousTablesAndRows) {
    ASSERT_EXIT(std::_Exit(checkReloadCleanup()), ::testing::ExitedWithCode(0), "");
}

void checkClear() {
    ServerRepository repository;
    GameServerInfoManager manager;
    manager.load(repository);
    manager.clear();
    if (manager.getMaxWorldID() != 0 || manager.getMaxServerGroupID() != 0)
        std::_Exit(1);
    manager.clear();
    manager.load(repository);
    manager.clear();
    std::_Exit(0);
}

TEST(ServerCatalogue, ClearResetsDimensionsAndAllowsRepeatedCleanupAndReload) {
    ASSERT_EXIT(checkClear(), ::testing::ExitedWithCode(0), "");
}

int checkAllocationFailure(std::size_t failAt, bool populated) {
    ServerRepository original;
    auto next = replacement();
    auto manager = std::make_unique<GameServerInfoManager>();
    if (populated)
        manager->load(original);
    const auto* previous = populated ? manager->getGameServerInfo(1, 0, 1) : nullptr;
    auto** view = populated ? manager->getGameServerInfos() : nullptr;
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        manager->load(next);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 128 && failed))
        return 1;
    if (failed &&
        (probe.outstanding() != 0 || (populated ? !retained(*manager, previous, view) : manager->getMaxWorldID() != 0)))
        return 2;
    manager->load(next);
    if (manager->getGameServerInfo(1, 3, 3)->getCastleFollowingServerID() != 45)
        return 3;
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 4;
}

TEST(ServerCatalogue, AllocationFailuresPreserveEmptyAndPopulatedTablesAndPermitRetry) {
    for (const bool populated : {false, true}) {
        for (std::size_t failAt = 1; failAt <= 128; ++failAt) {
            SCOPED_TRACE(populated);
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt, populated)), ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(ServerCatalogue, UnloadedAndOutOfRangeCountsAreBoundedAndLookupsRefuseMissingRows) {
    GameServerInfoManager manager;
    static_assert(!std::is_copy_constructible_v<GameServerInfoManager>);
    EXPECT_EQ(manager.getGameServerInfos(), nullptr);
    EXPECT_EQ(manager.getMaxWorldID(), 0);
    EXPECT_EQ(manager.getMaxServerGroupID(), 0);
    EXPECT_EQ(manager.getSize(0, 0), 0u);
    EXPECT_EQ(manager.getSize(255, 255), 0u);
    EXPECT_THROW(manager.getGameServerInfo(1, 0, 0), NoSuchElementException);
    ServerRepository repository;
    manager.load(repository);
    EXPECT_EQ(manager.getSize(255, 0), 0u);
    EXPECT_EQ(manager.getSize(1, 255), 0u);
    EXPECT_EQ(manager.getSize(0, 0), 0u);
    EXPECT_THROW(manager.getGameServerInfo(1, 0, 0), NoSuchElementException);
    EXPECT_THROW(manager.getGameServerInfo(1, 255, 1), NoSuchElementException);
    EXPECT_THROW(manager.getGameServerInfo(65535, 0, 1), NoSuchElementException);
}

TEST(ServerCatalogue, InvalidMaximaCannotChangeDimensionsOrReadLaterTables) {
    for (const bool group : {false, true}) {
        for (const int value : {-1, 256, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
            SCOPED_TRACE(group);
            SCOPED_TRACE(value);
            ServerRepository original;
            GameServerInfoManager manager;
            manager.load(original);
            const auto* previous = manager.getGameServerInfo(1, 0, 1);
            auto** view = manager.getGameServerInfos();
            auto next = replacement();
            (group ? next.maxGroup : next.maxWorld) = value;
            EXPECT_THROW(manager.load(next), Error);
            EXPECT_TRUE(retained(manager, previous, view));
            EXPECT_EQ(next.reads, group ? 1u : 2u);
        }
    }
}

TEST(ServerCatalogue, MissingMaximumRowsKeepTheStartupErrorAndStopLaterQueries) {
    for (const bool populated : {false, true}) {
        for (const bool group : {false, true}) {
            ServerRepository repository;
            GameServerInfoManager manager;
            if (populated)
                manager.load(repository);
            const auto* previous = populated ? manager.getGameServerInfo(1, 0, 1) : nullptr;
            auto** view = manager.getGameServerInfos();
            auto next = replacement();
            (group ? next.hasGroup : next.hasWorld) = false;
            try {
                manager.load(next);
                FAIL() << "Missing maximum accepted";
            } catch (const Error& error) {
                EXPECT_NE(error.toString().find("GameServerInfo TABLE does not exist!"), std::string::npos);
            }
            EXPECT_EQ(next.reads, group ? 1u : 2u);
            if (populated)
                EXPECT_TRUE(retained(manager, previous, view));
            else {
                EXPECT_EQ(manager.getGameServerInfos(), nullptr);
                EXPECT_EQ(manager.getMaxWorldID(), 0);
                EXPECT_EQ(manager.getMaxServerGroupID(), 0);
            }
            next.hasGroup = next.hasWorld = true;
            EXPECT_NO_THROW(manager.load(next));
        }
    }
}

TEST(ServerCatalogue, InvalidBaseFieldsCannotNarrowOrUseThePaddingAsAConfiguredWorld) {
    // Field: server, world, group, status, TCP, UDP.
    const std::vector<std::pair<unsigned, int>> cases = {{0, -1},  {0, 65536},
                                                         {1, -1},  {1, 256},
                                                         {1, 3},   {2, -1},
                                                         {2, 256}, {2, 3},
                                                         {3, -1},  {3, SERVER_DOWN + 1},
                                                         {4, -1},  {4, std::numeric_limits<int>::min()},
                                                         {5, -1},  {5, std::numeric_limits<int>::min()}};
    for (const auto [field, value] : cases) {
        SCOPED_TRACE(field);
        SCOPED_TRACE(value);
        ServerRepository original;
        GameServerInfoManager manager;
        manager.load(original);
        const auto* previous = manager.getGameServerInfo(1, 0, 1);
        auto** view = manager.getGameServerInfos();
        ServerRepository next;
        auto& row = next.servers.back(); // Refusal follows already prepared rows.
        switch (field) {
        case 0:
            row.serverID = value;
            break;
        case 1:
            row.worldID = value;
            break;
        case 2:
            row.groupID = value;
            break;
        case 3:
            row.stat = value;
            break;
        case 4:
            row.tcpPort = value;
            break;
        case 5:
            row.udpPort = value;
            break;
        }
        EXPECT_THROW(manager.load(next), Error);
        EXPECT_EQ(next.reads, 3u);
        EXPECT_TRUE(retained(manager, previous, view));
    }
}

TEST(ServerCatalogue, InvalidOverlayFieldsCannotAliasOtherWorldsGroupsOrFollowingServers) {
    for (const bool castle : {false, true}) {
        for (unsigned field = 0; field < (castle ? 3u : 2u); ++field) {
            const std::vector<int> invalid = field == 2 ? std::vector<int>{-1, 65536, std::numeric_limits<int>::max()}
                                                        : std::vector<int>{-1, 256, 3};
            for (const int value : invalid) {
                SCOPED_TRACE(castle);
                SCOPED_TRACE(field);
                SCOPED_TRACE(value);
                ServerRepository original;
                GameServerInfoManager manager;
                manager.load(original);
                const auto* previous = manager.getGameServerInfo(1, 0, 1);
                auto** view = manager.getGameServerInfos();
                ServerRepository next;
                if (castle) {
                    auto& row = next.castles.back();
                    (field == 0 ? row.worldID : field == 1 ? row.serverGroupID : row.followServerID) = value;
                } else {
                    next.nonPK.push_back({2, 1});
                    auto& row = next.nonPK.back();
                    (field == 0 ? row.worldID : row.serverGroupID) = value;
                }
                EXPECT_THROW(manager.load(next), Error);
                EXPECT_EQ(next.reads, castle ? 5u : 4u);
                EXPECT_TRUE(retained(manager, previous, view));
            }
        }
    }
}

TEST(ServerCatalogue, StoredBoundaryWidthsAndEveryTraversalSlotRemainUsable) {
    ServerRepository repository;
    repository.maxWorld = repository.maxGroup = 255;
    repository.servers = {{0, "Zero", "", 0, std::numeric_limits<int>::max(), 0, 0, SERVER_DOWN},
                          {65535, "Last", "192.0.2.1", 65535, 65536, 255, 255, SERVER_FULL},
                          {1, "Flag target", "192.0.2.2", 1, 2, 255, 255, SERVER_NORMAL}};
    repository.nonPK = {{255, 255}};
    repository.castles = {{255, 255, 65535}};
    GameServerInfoManager manager;
    manager.load(repository);
    EXPECT_EQ(manager.getMaxWorldID(), 257);
    EXPECT_EQ(manager.getMaxServerGroupID(), 256);
    EXPECT_EQ(manager.getSize(0, 0), 1u);
    EXPECT_EQ(manager.getSize(255, 255), 2u);
    const auto* zero = manager.getGameServerInfo(0, 0, 0);
    EXPECT_EQ(zero->getUDPPort(), static_cast<uint>(std::numeric_limits<int>::max()));
    EXPECT_EQ(zero->getTCPPort(), 0u);
    EXPECT_EQ(manager.getGameServerInfo(65535, 255, 255)->getUDPPort(), 65536u);
    EXPECT_EQ(manager.getGameServerInfo(1, 255, 255)->getCastleFollowingServerID(), 65535);
    for (int world = 0; world < manager.getMaxWorldID(); ++world) {
        ASSERT_NE(manager.getGameServerInfos()[world], nullptr);
        for (int group = 0; group < manager.getMaxServerGroupID(); ++group) {
            const auto& table = manager.getGameServerInfos()[world][group];
            if (world == 0 && group == 0)
                EXPECT_EQ(table.at(0), zero);
            else if (world != 255 || group != 255)
                EXPECT_TRUE(table.empty());
        }
    }
}

TEST(ServerCatalogue, RepeatedFlagsKeepLastCastleValueAndReloadResetsAbsentFlags) {
    ServerRepository repository;
    repository.nonPK.push_back(repository.nonPK.front());
    repository.castles.push_back({1, 0, 1000});
    GameServerInfoManager manager;
    manager.load(repository);
    EXPECT_TRUE(manager.getGameServerInfo(1, 0, 1)->isNonPKServer());
    EXPECT_EQ(manager.getGameServerInfo(1, 0, 1)->getCastleFollowingServerID(), 1000);
    EXPECT_FALSE(manager.getGameServerInfo(7, 0, 1)->isNonPKServer());
    EXPECT_EQ(manager.getGameServerInfo(7, 0, 1)->getCastleFollowingServerID(), 0);
    repository.nonPK.clear();
    repository.castles.clear();
    manager.load(repository);
    EXPECT_FALSE(manager.getGameServerInfo(1, 0, 1)->isNonPKServer());
    EXPECT_EQ(manager.getGameServerInfo(1, 0, 1)->getCastleFollowingServerID(), 0);
}

int checkRepositoryError(unsigned stage, unsigned kind, bool populated) {
    ServerRepository original;
    auto next = replacement();
    auto manager = std::make_unique<GameServerInfoManager>();
    if (populated)
        manager->load(original);
    const auto* previous = populated ? manager->getGameServerInfo(1, 0, 1) : nullptr;
    auto** view = manager->getGameServerInfos();
    const std::array failures{std::make_exception_ptr(std::runtime_error("query failed")),
                              std::make_exception_ptr(Error("query failed")),
                              std::make_exception_ptr(DatabaseError("query failed"))};
    next.failureStage = stage;
    next.failure = failures[kind];
    AllocationProbe probe;
    std::exception_ptr caught;
    try {
        manager->load(next);
    } catch (...) {
        caught = std::current_exception();
    }
    if (caught != next.failure || probe.outstanding() != 0 || next.reads != stage + 1 ||
        (populated ? !retained(*manager, previous, view) : manager->getGameServerInfos() != nullptr))
        return 1;
    next.failure = nullptr;
    manager->load(next);
    if (manager->getGameServerInfo(1, 3, 3)->getCastleFollowingServerID() != 45)
        return 2;
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 3;
}

TEST(ServerCatalogue, EveryQueryStagePreservesExceptionIdentityCleanupAndRetry) {
    for (const bool populated : {false, true}) {
        for (unsigned stage = 0; stage < 5; ++stage) {
            for (unsigned kind = 0; kind < 3; ++kind) {
                SCOPED_TRACE(populated);
                SCOPED_TRACE(stage);
                SCOPED_TRACE(kind);
                ASSERT_EXIT(std::_Exit(checkRepositoryError(stage, kind, populated)), ::testing::ExitedWithCode(0), "");
            }
        }
    }
}

class RefusingBuffer : public std::streambuf {
public:
    RefusingBuffer(std::string_view refused, unsigned occurrence) : refused(refused), occurrence(occurrence) {}

private:
    std::streamsize xsputn(const char* text, std::streamsize size) override {
        if (std::string_view(text, static_cast<std::size_t>(size)).find(refused) != std::string_view::npos &&
            ++matches == occurrence)
            return 0;
        return size;
    }
    int_type overflow(int_type value) override {
        return traits_type::not_eof(value);
    }
    std::string_view refused;
    unsigned occurrence;
    unsigned matches = 0;
};

int checkReportingFailure(std::string_view refused, unsigned occurrence) {
    ServerRepository original;
    auto next = replacement();
    next.servers.push_back(original.servers.front());
    next.nonPK.push_back({1, 0});
    next.castles.push_back({1, 0, 9});
    auto manager = std::make_unique<GameServerInfoManager>();
    manager->load(original);
    const auto* previous = manager->getGameServerInfo(1, 0, 1);
    auto** view = manager->getGameServerInfos();
    RefusingBuffer buffer(refused, occurrence);
    auto* previousBuffer = std::cout.rdbuf(&buffer);
    const auto previousExceptions = std::cout.exceptions();
    std::cout.exceptions(std::ios::badbit | std::ios::failbit);
    AllocationProbe probe;
    bool failed = false;
    try {
        manager->load(next);
    } catch (const std::ios_base::failure&) {
        failed = true;
    }
    std::cout.exceptions(std::ios::goodbit);
    std::cout.rdbuf(previousBuffer);
    std::cout.clear();
    std::cout.exceptions(previousExceptions);
    if (!failed || !retained(*manager, previous, view) || probe.outstanding() != 0)
        return 1;
    manager->load(next);
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(ServerCatalogue, ThrowingDiagnosticsCannotPublishDimensionsBaseRowsOrPartialFlags) {
    ASSERT_EXIT(std::_Exit(checkReportingFailure("MAX SERVER GROUP", 1)), ::testing::ExitedWithCode(0), "");
    for (const auto text : {"NonPK set", "follows"}) {
        for (unsigned occurrence = 1; occurrence <= 2; ++occurrence) {
            SCOPED_TRACE(text);
            SCOPED_TRACE(occurrence);
            ASSERT_EXIT(std::_Exit(checkReportingFailure(text, occurrence)), ::testing::ExitedWithCode(0), "");
        }
    }
}

int checkRefusalCleanup(unsigned kind) {
    ServerRepository original;
    auto next = replacement();
    if (kind == 0)
        next.servers.push_back(next.servers.front());
    else if (kind == 1)
        next.nonPK.push_back({1, 0});
    else
        next.castles.push_back({1, 0, 1});
    auto manager = std::make_unique<GameServerInfoManager>();
    manager->load(original);
    const auto* previous = manager->getGameServerInfo(1, 0, 1);
    auto** view = manager->getGameServerInfos();
    AllocationProbe probe;
    bool refused = false;
    try {
        manager->load(next);
    } catch (const DuplicatedException&) {
        refused = kind == 0;
    } catch (const NoSuchElementException&) {
        refused = kind != 0;
    }
    if (!refused || !retained(*manager, previous, view) || probe.outstanding() != 0)
        return 1;
    if (kind == 0)
        next.servers.pop_back();
    else if (kind == 1)
        next.nonPK.pop_back();
    else
        next.castles.pop_back();
    manager->load(next);
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(ServerCatalogue, DuplicateAndMissingOverlayRefusalReleasePreparationAndPermitRetry) {
    for (unsigned kind = 0; kind < 3; ++kind)
        ASSERT_EXIT(std::_Exit(checkRefusalCleanup(kind)), ::testing::ExitedWithCode(0), "");
}

TEST(ServerCatalogue, EmptyRowsReplaceThePreviousCatalogueWithTheReportedDimensions) {
    ServerRepository original;
    GameServerInfoManager manager;
    manager.load(original);
    auto next = replacement();
    next.servers.clear();
    next.nonPK.clear();
    next.castles.clear();
    manager.load(next);
    EXPECT_EQ(manager.getMaxWorldID(), 5);
    EXPECT_EQ(manager.getMaxServerGroupID(), 4);
    EXPECT_EQ(manager.getSize(1, 0), 0u);
    EXPECT_EQ(manager.getSize(3, 3), 0u);
    EXPECT_THROW(manager.getGameServerInfo(1, 0, 1), NoSuchElementException);
}

TEST(ServerCatalogue, ClearAllocatesNothingAndRepeatedLifetimesReleaseEveryRowAndIndex) {
    ASSERT_EXIT(
        {
            ServerRepository repository;
            GameServerInfoManager manager;
            manager.load(repository);
            AllocationProbe probe(1);
            manager.clear();
            manager.clear();
            if (probe.rejected() || probe.attempts() != 0 || manager.getGameServerInfos() != nullptr)
                std::_Exit(1);
            probe.stopFailing();
            for (unsigned repeat = 0; repeat < 8; ++repeat) {
                manager.load(repository);
                manager.load(repository);
                manager.clear();
                if (probe.outstanding() != 0 || manager.getSize(0, 0) != 0)
                    std::_Exit(2);
            }
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ServerCatalogue, DebugOutputIncludesWorldZeroAndReportsStatusRatherThanGroupID) {
    ServerRepository repository;
    repository.servers.resize(1);
    repository.servers.front().worldID = 0;
    repository.servers.front().stat = SERVER_DOWN;
    repository.nonPK.clear();
    repository.castles.clear();
    GameServerInfoManager manager;
    manager.load(repository);
    EXPECT_NE(manager.toString().find("First server"), std::string::npos);
    EXPECT_NE(manager.getGameServerInfo(1, 0, 0)->toString().find("ServerStat:5)"), std::string::npos);
}

} // namespace
