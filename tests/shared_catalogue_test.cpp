#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "GameServerGroupInfoManager.h"
#include "SharedGameServerInfoManager.h"
#include "repository/SharedConfigRepository.h"
#include "support/AllocationProbe.h"

namespace {

class CatalogueRepository : public SharedConfigRepository {
public:
    bool loadMaxGameServerGroupWorldID(int& maximum) override {
        if (maxFailure)
            std::rethrow_exception(maxFailure);
        maximum = maxWorld;
        return hasWorlds;
    }
    std::vector<SharedGameServerGroupRow> loadGameServerGroups() override {
        if (rowFailure)
            std::rethrow_exception(rowFailure);
        return groups;
    }
    bool loadMaxGameServerGroupID(int worldID, int& maximum) override {
        requestedWorld = worldID;
        if (maxFailure)
            std::rethrow_exception(maxFailure);
        maximum = maxGroup;
        return hasGroups;
    }
    std::vector<SharedGameServerRow> loadGameServers() override {
        if (rowFailure)
            std::rethrow_exception(rowFailure);
        return servers;
    }
    std::vector<SharedResurrectLocationRow> loadResurrectLocations() override {
        std::abort();
    }
    std::vector<SharedStringRow> loadStrings() override {
        std::abort();
    }

    int requestedWorld = -1;
    int maxWorld = 1;
    int maxGroup = 2;
    bool hasWorlds = true;
    bool hasGroups = true;
    std::exception_ptr maxFailure;
    std::exception_ptr rowFailure;
    std::vector<SharedGameServerGroupRow> groups = {{1, 1, "first"}, {1, 2, "second"}};
    std::vector<SharedGameServerRow> servers = {{101, "first", "127.0.0.1", 9998, 9997, 1, 0, 1},
                                                {102, "second", "127.0.0.2", 9996, 9995, 2, 0, 1},
                                                {201, "other world", "127.0.0.3", 9994, 9993, 2, 0, 2}};
};

TEST(SharedCatalogue, LoadsGroupsFromAnExplicitRepositoryWithoutStartup) {
    CatalogueRepository repository;
    GameServerGroupInfoManager groups;
    groups.load(repository);

    EXPECT_EQ(groups.getSize(1), 2u);
    const auto* first = groups.getGameServerGroupInfo(1, 1);
    EXPECT_EQ(first->getWorldID(), 1);
    EXPECT_EQ(first->getGroupID(), 1);
    EXPECT_EQ(first->getGroupName(), "first");
    EXPECT_EQ(groups.getGameServerGroupInfo(2, 1)->getGroupName(), "second");
}

TEST(SharedCatalogue, LoadsOnlyTheRequestedWorldAndPreservesServerFields) {
    CatalogueRepository repository;
    SharedGameServerInfoManager servers;
    servers.load(repository, 1);

    EXPECT_EQ(repository.requestedWorld, 1);
    EXPECT_EQ(servers.getMaxServerGroupID(), 3);
    EXPECT_EQ(servers.getSize(1), 1u);
    EXPECT_EQ(servers.getSize(2), 1u);
    const auto* first = servers.getGameServerInfo(101, 1);
    EXPECT_EQ(first->getServerID(), 101);
    EXPECT_EQ(first->getWorldID(), 1);
    EXPECT_EQ(first->getGroupID(), 1);
    EXPECT_EQ(first->getNickname(), "first");
    EXPECT_EQ(first->getIP(), "127.0.0.1");
    EXPECT_EQ(first->getTCPPort(), 9998u);
    EXPECT_EQ(first->getUDPPort(), 9997u);
    EXPECT_EQ(static_cast<int>(first->getServerStat()), 0);
    EXPECT_THROW(servers.getGameServerInfo(201, 2), NoSuchElementException);
}

TEST(SharedCatalogue, FirstGroupTableAllocationFailureLeavesASafelyDestructibleManager) {
    ASSERT_EXIT(
        {
            ::alarm(3);
            CatalogueRepository repository;
            auto groups = std::make_unique<GameServerGroupInfoManager>();
            AllocationProbe probe(1);
            bool failed = false;
            try {
                groups->load(repository);
            } catch (const std::bad_alloc&) {
                failed = true;
            }
            probe.stopFailing();
            if (!failed || !probe.rejected())
                std::_Exit(1);
            groups.reset();
            std::_Exit(probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedCatalogue, FirstServerTableAllocationFailureLeavesASafelyDestructibleManager) {
    ASSERT_EXIT(
        {
            ::alarm(3);
            CatalogueRepository repository;
            auto servers = std::make_unique<SharedGameServerInfoManager>();
            AllocationProbe probe(1);
            bool failed = false;
            try {
                servers->load(repository, 1);
            } catch (const std::bad_alloc&) {
                failed = true;
            }
            probe.stopFailing();
            if (!failed || !probe.rejected())
                std::_Exit(1);
            servers.reset();
            std::_Exit(probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedCatalogue, ReloadingGroupsReleasesThePreviousCatalogue) {
    ASSERT_EXIT(
        {
            CatalogueRepository repository;
            AllocationProbe probe;
            {
                GameServerGroupInfoManager groups;
                groups.load(repository);
                groups.load(repository);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedCatalogue, ReloadingServersReleasesThePreviousCatalogue) {
    ASSERT_EXIT(
        {
            CatalogueRepository repository;
            {
                // Warm the loader's diagnostic stream before allocation tracking.
                SharedGameServerInfoManager warmup;
                warmup.load(repository, 1);
            }
            AllocationProbe probe;
            {
                SharedGameServerInfoManager servers;
                servers.load(repository, 1);
                servers.load(repository, 1);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

int checkGroupAllocationFailure(std::size_t failAt) {
    CatalogueRepository original;
    CatalogueRepository replacement;
    replacement.groups = {{0, 5, std::string(96, 'z')}, {1, 7, std::string(128, 'n')}};
    auto groups = std::make_unique<GameServerGroupInfoManager>();
    groups->load(original);
    const auto* previous = groups->getGameServerGroupInfo(1, 1);
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        groups->load(replacement);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 96 && probe.rejected()))
        return 1;
    if (failed) {
        if (groups->getGameServerGroupInfo(1, 1) != previous || previous->getGroupName() != "first" ||
            groups->getSize(1) != 2 || probe.outstanding() != 0)
            return 2;
        groups->load(replacement);
    }
    if (groups->getSize(0) != 1 || groups->getSize(1) != 1 ||
        groups->getGameServerGroupInfo(7, 1)->getGroupName() != replacement.groups[1].groupName)
        return 3;
    groups.reset();
    return probe.outstanding() == 0 ? 0 : 4;
}

TEST(SharedCatalogue, GroupAllocationFailuresPreserveThePreviousCatalogueAndPermitRetry) {
    for (std::size_t failAt = 1; failAt <= 96; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkGroupAllocationFailure(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

int checkServerAllocationFailure(std::size_t failAt) {
    CatalogueRepository original;
    CatalogueRepository replacement;
    replacement.maxGroup = 3;
    replacement.servers = {{111, std::string(96, 'a'), std::string(80, 'b'), 9998, 9997, 2, SERVER_BUSY, 1},
                           {112, std::string(128, 'c'), std::string(64, 'd'), 9996, 9995, 3, SERVER_DOWN, 1}};
    auto servers = std::make_unique<SharedGameServerInfoManager>();
    servers->load(original, 1); // Also warm the diagnostic stream before the probe.
    const auto* previous = servers->getGameServerInfo(101, 1);
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        servers->load(replacement, 1);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 96 && probe.rejected()))
        return 1;
    if (failed) {
        if (servers->getGameServerInfo(101, 1) != previous || previous->getNickname() != "first" ||
            servers->getMaxServerGroupID() != 3 || probe.outstanding() != 0)
            return 2;
        servers->load(replacement, 1);
    }
    if (servers->getMaxServerGroupID() != 4 || servers->getSize(1) != 0 || servers->getSize(2) != 1 ||
        servers->getSize(3) != 1 ||
        servers->getGameServerInfo(111, 2)->getNickname() != replacement.servers[0].nickname ||
        servers->getGameServerInfo(112, 3)->getIP() != replacement.servers[1].ip)
        return 3;
    servers.reset();
    return probe.outstanding() == 0 ? 0 : 4;
}

TEST(SharedCatalogue, ServerAllocationFailuresPreserveThePreviousCatalogueAndPermitRetry) {
    for (std::size_t failAt = 1; failAt <= 96; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkServerAllocationFailure(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(SharedCatalogue, MissingAndInvalidDimensionsPreserveBothCatalogues) {
    CatalogueRepository original;
    GameServerGroupInfoManager groups;
    SharedGameServerInfoManager servers;
    groups.load(original);
    servers.load(original, 1);
    const auto* previousGroup = groups.getGameServerGroupInfo(1, 1);
    const auto* previousServer = servers.getGameServerInfo(101, 1);
    CatalogueRepository rejected;
    rejected.hasWorlds = rejected.hasGroups = false;
    EXPECT_THROW(groups.load(rejected), Error);
    EXPECT_THROW(servers.load(rejected, 1), Error);
    rejected.hasWorlds = rejected.hasGroups = true;
    for (const int invalid : {-1, 256, std::numeric_limits<int>::max()}) {
        SCOPED_TRACE(invalid);
        rejected.maxWorld = rejected.maxGroup = invalid;
        EXPECT_THROW(groups.load(rejected), Error);
        EXPECT_THROW(servers.load(rejected, 1), Error);
        EXPECT_EQ(groups.getGameServerGroupInfo(1, 1), previousGroup);
        EXPECT_EQ(servers.getGameServerInfo(101, 1), previousServer);
    }
}

TEST(SharedCatalogue, InvalidWorldArgumentsDoNotQueryOrReplaceTheCatalogue) {
    CatalogueRepository repository;
    SharedGameServerInfoManager servers;
    servers.load(repository, 1);
    const auto* previous = servers.getGameServerInfo(101, 1);
    for (const int invalid : {-1, 256, std::numeric_limits<int>::max()}) {
        SCOPED_TRACE(invalid);
        repository.requestedWorld = -1;
        EXPECT_THROW(servers.load(repository, invalid), Error);
        EXPECT_EQ(repository.requestedWorld, -1);
        EXPECT_EQ(servers.getGameServerInfo(101, 1), previous);
    }
}

TEST(SharedCatalogue, InvalidGroupRowsAreRejectedBeforeNarrowingOrPublication) {
    CatalogueRepository original;
    GameServerGroupInfoManager groups;
    groups.load(original);
    const auto* previous = groups.getGameServerGroupInfo(1, 1);
    for (int failure = 0; failure < 5; ++failure) {
        SCOPED_TRACE(failure);
        CatalogueRepository rejected;
        auto& row = rejected.groups.back(); // Fail after another row was prepared.
        switch (failure) {
        case 0:
            row.worldID = -1;
            break;
        case 1:
            row.worldID = 2;
            break;
        case 2:
            row.worldID = 256;
            break;
        case 3:
            row.groupID = -1;
            break;
        case 4:
            row.groupID = 256;
            break;
        }
        EXPECT_THROW(groups.load(rejected), Error);
        EXPECT_EQ(groups.getGameServerGroupInfo(1, 1), previous);
        EXPECT_EQ(groups.getSize(1), 2u);
    }
}

TEST(SharedCatalogue, InvalidServerRowsAreRejectedBeforeNarrowingOrPublication) {
    CatalogueRepository original;
    SharedGameServerInfoManager servers;
    servers.load(original, 1);
    const auto* previous = servers.getGameServerInfo(101, 1);
    for (int failure = 0; failure < 7; ++failure) {
        SCOPED_TRACE(failure);
        CatalogueRepository rejected;
        auto& row = rejected.servers[1];
        switch (failure) {
        case 0:
            row.serverID = -1;
            break;
        case 1:
            row.serverID = 65536;
            break;
        case 2:
            row.groupID = -1;
            break;
        case 3:
            row.groupID = 3;
            break;
        case 4:
            row.groupID = 256;
            break;
        case 5:
            row.stat = -1;
            break;
        case 6:
            row.stat = SERVER_DOWN + 1;
            break;
        }
        EXPECT_THROW(servers.load(rejected, 1), Error);
        EXPECT_EQ(servers.getGameServerInfo(101, 1), previous);
        EXPECT_EQ(servers.getMaxServerGroupID(), 3);
    }
}

TEST(SharedCatalogue, DuplicateRowsRefuseTheReplacementInsteadOfPublishingAPrefix) {
    CatalogueRepository original;
    GameServerGroupInfoManager groups;
    SharedGameServerInfoManager servers;
    groups.load(original);
    servers.load(original, 1);
    const auto* previousGroup = groups.getGameServerGroupInfo(1, 1);
    const auto* previousServer = servers.getGameServerInfo(101, 1);
    CatalogueRepository duplicates;
    duplicates.groups.push_back(duplicates.groups.front());
    duplicates.servers.push_back(duplicates.servers.front());
    EXPECT_THROW(groups.load(duplicates), DuplicatedException);
    EXPECT_THROW(servers.load(duplicates, 1), DuplicatedException);
    EXPECT_EQ(groups.getGameServerGroupInfo(1, 1), previousGroup);
    EXPECT_EQ(servers.getGameServerInfo(101, 1), previousServer);
}

TEST(SharedCatalogue, IdenticalIDsInDifferentParentsRemainIndependent) {
    CatalogueRepository repository;
    repository.groups = {{0, 1, "zero world"}, {1, 1, "first world"}};
    repository.servers[1].serverID = 101;
    GameServerGroupInfoManager groups;
    SharedGameServerInfoManager servers;
    groups.load(repository);
    servers.load(repository, 1);
    EXPECT_EQ(groups.getGameServerGroupInfo(1, 0)->getGroupName(), "zero world");
    EXPECT_EQ(groups.getGameServerGroupInfo(1, 1)->getGroupName(), "first world");
    EXPECT_EQ(servers.getGameServerInfo(101, 1)->getNickname(), "first");
    EXPECT_EQ(servers.getGameServerInfo(101, 2)->getNickname(), "second");
}

TEST(SharedCatalogue, ZeroAndMaximumIDsAreRepresentedWithoutDimensionOverflow) {
    CatalogueRepository repository;
    repository.maxWorld = repository.maxGroup = 255;
    repository.groups = {{0, 0, "zero"}, {255, 255, "maximum"}};
    repository.servers = {{0, "zero", "127.0.0.1", 0, 0, 0, SERVER_FREE, 255},
                          {65535, "maximum", "127.0.0.2", 65535, 65535, 255, SERVER_DOWN, 255}};
    GameServerGroupInfoManager groups;
    SharedGameServerInfoManager servers;
    groups.load(repository);
    servers.load(repository, 255);
    EXPECT_EQ(groups.getGameServerGroupInfo(0, 0)->getGroupName(), "zero");
    EXPECT_EQ(groups.getGameServerGroupInfo(255, 255)->getGroupName(), "maximum");
    EXPECT_EQ(servers.getMaxServerGroupID(), 256);
    EXPECT_EQ(servers.getGameServerInfo(0, 0)->getTCPPort(), 0u);
    EXPECT_EQ(servers.getGameServerInfo(65535, 255)->getUDPPort(), 65535u);
    EXPECT_EQ(servers.getGameServerInfo(65535, 255)->getServerStat(), SERVER_DOWN);
}

TEST(SharedCatalogue, OtherWorldsAreFilteredBeforeRowValidation) {
    CatalogueRepository repository;
    repository.servers.back().serverID = -1;
    repository.servers.back().groupID = std::numeric_limits<int>::max();
    repository.servers.back().stat = -1;
    SharedGameServerInfoManager servers;
    EXPECT_NO_THROW(servers.load(repository, 1));
    EXPECT_EQ(servers.getSize(2), 1u);
}

TEST(SharedCatalogue, EmptyAndOutOfRangeLookupsAreRefused) {
    GameServerGroupInfoManager groups;
    SharedGameServerInfoManager servers;
    EXPECT_THROW(groups.getGameServerGroupInfo(0, 0), NoSuchElementException);
    EXPECT_THROW(groups.getSize(0), NoSuchElementException);
    EXPECT_THROW(servers.getGameServerInfo(0, 0), NoSuchElementException);
    EXPECT_THROW(servers.getSize(0), NoSuchElementException);
    CatalogueRepository repository;
    groups.load(repository);
    servers.load(repository, 1);
    EXPECT_THROW(groups.getGameServerGroupInfo(1, 255), NoSuchElementException);
    EXPECT_THROW(groups.getGameServerGroupInfo(99, 1), NoSuchElementException);
    EXPECT_THROW(groups.getSize(255), NoSuchElementException);
    EXPECT_THROW(servers.getGameServerInfo(101, 255), NoSuchElementException);
    EXPECT_THROW(servers.getGameServerInfo(99, 1), NoSuchElementException);
    EXPECT_THROW(servers.getSize(255), NoSuchElementException);
}

TEST(SharedCatalogue, RepositoryExceptionsPreserveTheOriginalExceptionAndBothCatalogues) {
    CatalogueRepository original;
    GameServerGroupInfoManager groups;
    SharedGameServerInfoManager servers;
    groups.load(original);
    servers.load(original, 1);
    const auto* previousGroup = groups.getGameServerGroupInfo(1, 1);
    const auto* previousServer = servers.getGameServerInfo(101, 1);
    const auto failure = std::make_exception_ptr(std::runtime_error("repository failed"));
    for (bool failMax : {true, false}) {
        CatalogueRepository rejected;
        (failMax ? rejected.maxFailure : rejected.rowFailure) = failure;
        try {
            groups.load(rejected);
            FAIL() << "expected the repository failure";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        try {
            servers.load(rejected, 1);
            FAIL() << "expected the repository failure";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        EXPECT_EQ(groups.getGameServerGroupInfo(1, 1), previousGroup);
        EXPECT_EQ(servers.getGameServerInfo(101, 1), previousServer);
    }
}

TEST(SharedCatalogue, GroupRowDatabaseErrorsKeepTheirExistingTranslation) {
    CatalogueRepository repository;
    GameServerGroupInfoManager groups;
    groups.load(repository);
    const auto* previous = groups.getGameServerGroupInfo(1, 1);
    repository.rowFailure = std::make_exception_ptr(DatabaseError("database failed"));
    try {
        groups.load(repository);
        FAIL() << "expected the translated database error";
    } catch (const Error& error) {
        EXPECT_NE(error.toString().find("GameServerGroupInfoManager::load : database failed"), std::string::npos);
    }
    EXPECT_EQ(groups.getGameServerGroupInfo(1, 1), previous);
}

class RefusingBuffer : public std::streambuf {
public:
    bool rejected = false;

private:
    std::streamsize xsputn(const char*, std::streamsize) override {
        rejected = true;
        return 0;
    }
};

TEST(SharedCatalogue, FailedOptionalDiagnosticsCannotPreventPublishingACompleteServerCatalogue) {
    ASSERT_EXIT(
        {
            ::setenv("DARKEDEN_TRACE", "1", 1);
            CatalogueRepository repository;
            SharedGameServerInfoManager servers;
            servers.load(repository, 1);
            repository.maxGroup = 3;
            RefusingBuffer buffer;
            auto* previousBuffer = std::cerr.rdbuf(&buffer);
            const auto previousExceptions = std::cerr.exceptions();
            std::cerr.exceptions(std::ios::badbit | std::ios::failbit);
            bool failed = false;
            try {
                servers.load(repository, 1);
            } catch (...) {
                failed = true;
            }
            const bool streamHealthy = std::cerr.good();
            std::cerr.exceptions(std::ios::goodbit);
            std::cerr.rdbuf(previousBuffer);
            std::cerr.clear();
            std::cerr.exceptions(previousExceptions);
            const bool published = servers.getMaxServerGroupID() == 4 &&
                                   servers.getGameServerInfo(101, 1)->getNickname() == "first" &&
                                   servers.getGameServerInfo(102, 2)->getNickname() == "second";
            std::_Exit(!failed && buffer.rejected && streamHealthy && published ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

} // namespace
