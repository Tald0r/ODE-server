#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <string_view>

#include "DatabaseError.h"
#include "GameWorldInfoManager.h"
#include "repository/ServerInfoRepository.h"
#include "support/AllocationProbe.h"

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
        if (failure)
            std::rethrow_exception(failure);
        return worlds;
    }

    std::exception_ptr failure;
    std::vector<ServerInfoWorldRow> worlds = {{1, "first world", WORLD_OPEN}, {2, "second world", WORLD_CLOSE}};
};

TEST(WorldCatalogue, LoadsProductionWorldsFromAnExplicitRepository) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    EXPECT_EQ(worlds.getSize(), 2u);
    const auto* first = worlds.getGameWorldInfo(1);
    EXPECT_EQ(first->getID(), 1);
    EXPECT_EQ(first->getName(), "first world");
    EXPECT_EQ(first->getStatus(), WORLD_OPEN);
    EXPECT_EQ(worlds.getGameWorldInfo(2)->getStatus(), WORLD_CLOSE);
    EXPECT_THROW(worlds.getGameWorldInfo(3), NoSuchElementException);
}

TEST(WorldCatalogue, DatabaseErrorsKeepTheirExistingTranslation) {
    WorldRepository repository;
    repository.failure = std::make_exception_ptr(DatabaseError("database failed"));
    GameWorldInfoManager worlds;
    try {
        worlds.load(repository);
        FAIL() << "expected the translated database error";
    } catch (const Error& error) {
        EXPECT_NE(error.toString().find("GameWorldInfoManager::load : database failed"), std::string::npos);
    }
    EXPECT_EQ(worlds.getSize(), 0u);
}

TEST(WorldCatalogue, FailedRepositoryReadPreservesThePreviousCatalogue) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    const auto* previous = worlds.getGameWorldInfo(1);
    repository.failure = std::make_exception_ptr(std::runtime_error("fetch failed"));
    EXPECT_THROW(worlds.load(repository), std::runtime_error);
    EXPECT_EQ(worlds.getSize(), 2u);
    EXPECT_EQ(worlds.getGameWorldInfo(1), previous);
    EXPECT_EQ(worlds.getGameWorldInfo(2)->getName(), "second world");
}

TEST(WorldCatalogue, DuplicateRowsRefuseTheReplacementAndRetainThePreviousCatalogue) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    const auto* previous = worlds.getGameWorldInfo(1);
    repository.worlds = {{3, "replacement", WORLD_OPEN}, {3, "duplicate", WORLD_CLOSE}};
    EXPECT_THROW(worlds.load(repository), DuplicatedException);
    EXPECT_THROW(worlds.getGameWorldInfo(3), NoSuchElementException);
    EXPECT_EQ(worlds.getSize(), 2u);
    EXPECT_EQ(worlds.getGameWorldInfo(1), previous);
}

int checkAllocationCleanup(std::size_t failAt) {
    WorldRepository repository;
    repository.worlds[0].name = std::string(96, 'a');
    repository.worlds[1].name = std::string(128, 'b');
    {
        GameWorldInfoManager warmup;
        warmup.load(repository);
    }
    auto worlds = std::make_unique<GameWorldInfoManager>();
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        worlds->load(repository);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed))
        return 1;
    if (failed) {
        if (worlds->getSize() != 0 || probe.outstanding() != 0)
            return 2;
        worlds->load(repository);
    }
    if (worlds->getSize() != 2 || worlds->getGameWorldInfo(1)->getName() != repository.worlds[0].name ||
        worlds->getGameWorldInfo(2)->getStatus() != WORLD_CLOSE)
        return 3;
    worlds.reset();
    return probe.outstanding() == 0 ? 0 : 4;
}

TEST(WorldCatalogue, AllocationFailuresReleaseEveryPreparedRow) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkAllocationCleanup(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

int checkReplacementAllocationFailure(std::size_t failAt) {
    WorldRepository original;
    WorldRepository replacement;
    replacement.worlds = {{3, std::string(96, 'a'), WORLD_CLOSE}, {4, std::string(128, 'b'), WORLD_OPEN}};
    auto worlds = std::make_unique<GameWorldInfoManager>();
    worlds->load(original);
    const auto* previous = worlds->getGameWorldInfo(1);
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        worlds->load(replacement);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed))
        return 1;
    if (failed) {
        if (worlds->getSize() != 2 || worlds->getGameWorldInfo(1) != previous || previous->getName() != "first world" ||
            worlds->getGameWorldInfo(2)->getStatus() != WORLD_CLOSE || probe.outstanding() != 0)
            return 2;
        try {
            worlds->getGameWorldInfo(3);
            return 3;
        } catch (const NoSuchElementException&) {
        }
        worlds->load(replacement);
    }
    if (worlds->getSize() != 2 || worlds->getGameWorldInfo(3)->getName() != replacement.worlds[0].name ||
        worlds->getGameWorldInfo(4)->getName() != replacement.worlds[1].name)
        return 4;
    try {
        worlds->getGameWorldInfo(1);
        return 5;
    } catch (const NoSuchElementException&) {
    }
    worlds.reset();
    return probe.outstanding() == 0 ? 0 : 6;
}

TEST(WorldCatalogue, ReloadAllocationFailuresPreserveBorrowedRowsAndPermitRetry) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkReplacementAllocationFailure(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(WorldCatalogue, InvalidIDsAndStatusesAreRefusedBeforeNarrowingOrPublication) {
    WorldRepository original;
    GameWorldInfoManager worlds;
    worlds.load(original);
    const auto* previous = worlds.getGameWorldInfo(1);
    for (int failure = 0; failure < 5; ++failure) {
        SCOPED_TRACE(failure);
        WorldRepository rejected;
        rejected.worlds.front().name = "replacement";
        auto& row = rejected.worlds.back();
        switch (failure) {
        case 0:
            row.id = -1;
            break;
        case 1:
            row.id = 256;
            break;
        case 2:
            row.id = std::numeric_limits<int>::max();
            break;
        case 3:
            row.stat = -1;
            break;
        case 4:
            row.stat = WORLD_CLOSE + 1;
            break;
        }
        EXPECT_THROW(worlds.load(rejected), Error);
        EXPECT_EQ(worlds.getSize(), 2u);
        EXPECT_EQ(worlds.getGameWorldInfo(1), previous);
        EXPECT_EQ(worlds.getGameWorldInfo(1)->getName(), "first world");
        EXPECT_EQ(worlds.getGameWorldInfo(2)->getStatus(), WORLD_CLOSE);
    }
}

TEST(WorldCatalogue, ZeroAndMaximumIDsPreserveExactNamesAndBothStatuses) {
    WorldRepository repository;
    repository.worlds = {{0, "", WORLD_OPEN}, {255, std::string("first\0second", 12), WORLD_CLOSE}};
    GameWorldInfoManager worlds;
    worlds.load(repository);
    EXPECT_EQ(worlds.getSize(), 2u);
    EXPECT_EQ(worlds.getGameWorldInfo(0)->getID(), 0);
    EXPECT_EQ(worlds.getGameWorldInfo(0)->getName(), "");
    EXPECT_EQ(worlds.getGameWorldInfo(0)->getStatus(), WORLD_OPEN);
    EXPECT_EQ(worlds.getGameWorldInfo(255)->getID(), 255);
    EXPECT_EQ(worlds.getGameWorldInfo(255)->getName(), repository.worlds[1].name);
    EXPECT_EQ(worlds.getGameWorldInfo(255)->getStatus(), WORLD_CLOSE);
}

TEST(WorldCatalogue, ReloadUpdatesExistingWorldsAndDropsRemovedWorlds) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    repository.worlds = {{1, "updated world", WORLD_CLOSE}, {3, "new world", WORLD_OPEN}};
    worlds.load(repository);
    EXPECT_EQ(worlds.getSize(), 2u);
    EXPECT_EQ(worlds.getGameWorldInfo(1)->getName(), "updated world");
    EXPECT_EQ(worlds.getGameWorldInfo(1)->getStatus(), WORLD_CLOSE);
    EXPECT_THROW(worlds.getGameWorldInfo(2), NoSuchElementException);
    EXPECT_EQ(worlds.getGameWorldInfo(3)->getName(), "new world");
    const auto text = worlds.toString();
    EXPECT_NE(text.find("GameWorldInfo(WorldID: 1,Name:updated world)"), std::string::npos);
    EXPECT_NE(text.find("GameWorldInfo(WorldID: 3,Name:new world)"), std::string::npos);
}

TEST(WorldCatalogue, EmptyLoadsReplacePreviousRowsAndAllowAnotherLoad) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    EXPECT_EQ(worlds.getSize(), 0u);
    EXPECT_THROW(worlds.getGameWorldInfo(0), NoSuchElementException);
    EXPECT_EQ(worlds.toString(), "GameWorldInfoManager(\nEMPTY)");
    worlds.load(repository);
    repository.worlds.clear();
    worlds.load(repository);
    EXPECT_EQ(worlds.getSize(), 0u);
    EXPECT_THROW(worlds.getGameWorldInfo(1), NoSuchElementException);
    EXPECT_EQ(worlds.toString(), "GameWorldInfoManager(\nEMPTY)");
    repository.worlds = {{1, "retry", WORLD_OPEN}};
    worlds.load(repository);
    EXPECT_EQ(worlds.getSize(), 1u);
    EXPECT_EQ(worlds.getGameWorldInfo(1)->getName(), "retry");
}

TEST(WorldCatalogue, FailedDatabaseReadRetainsPreviousRowsAndAllowsRetry) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    const auto* previous = worlds.getGameWorldInfo(1);
    repository.failure = std::make_exception_ptr(DatabaseError("fetch failed"));
    try {
        worlds.load(repository);
        FAIL() << "expected the translated database error";
    } catch (const Error& error) {
        EXPECT_NE(error.toString().find("GameWorldInfoManager::load : fetch failed"), std::string::npos);
    }
    EXPECT_EQ(worlds.getGameWorldInfo(1), previous);
    EXPECT_EQ(worlds.getSize(), 2u);
    repository.failure = nullptr;
    repository.worlds[0].name = "retry";
    worlds.load(repository);
    EXPECT_EQ(worlds.getGameWorldInfo(1)->getName(), "retry");
}

TEST(WorldCatalogue, OtherRepositoryExceptionsRetainTheirIdentityAndPreviousRows) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    const auto* previous = worlds.getGameWorldInfo(1);
    for (const auto& failure : {std::make_exception_ptr(std::runtime_error("standard failure")),
                                std::make_exception_ptr(Error("legacy failure"))}) {
        repository.failure = failure;
        try {
            worlds.load(repository);
            FAIL() << "expected the original exception";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        EXPECT_EQ(worlds.getSize(), 2u);
        EXPECT_EQ(worlds.getGameWorldInfo(1), previous);
        EXPECT_EQ(worlds.getGameWorldInfo(1)->getName(), "first world");
    }
}

TEST(WorldCatalogue, RepeatedLoadsAndDestructionReleaseAllOwnedAllocations) {
    ASSERT_EXIT(
        {
            WorldRepository repository;
            repository.worlds.front().name = std::string(128, 'a');
            {
                GameWorldInfoManager warmup;
                warmup.load(repository);
            }
            AllocationProbe probe;
            {
                GameWorldInfoManager worlds;
                worlds.load(repository);
                worlds.load(repository);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(WorldCatalogue, DuplicateRefusalReleasesPreparedRowsAndAllowsRetry) {
    ASSERT_EXIT(
        {
            WorldRepository original;
            WorldRepository duplicates;
            duplicates.worlds[0].name = std::string(96, 'a');
            duplicates.worlds[1].name = std::string(128, 'b');
            duplicates.worlds.back().id = 1;
            auto worlds = std::make_unique<GameWorldInfoManager>();
            worlds->load(original);
            const auto* previous = worlds->getGameWorldInfo(1);
            AllocationProbe probe;
            bool refused = false;
            try {
                worlds->load(duplicates);
            } catch (const DuplicatedException&) {
                refused = true;
            }
            if (!refused || worlds->getGameWorldInfo(1) != previous || probe.outstanding() != 0)
                std::_Exit(1);
            worlds->load(original);
            worlds.reset();
            std::_Exit(probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

class RefusingBuffer : public std::streambuf {
public:
    explicit RefusingBuffer(std::string_view refused) : refused(refused) {}
    bool rejected = false;

private:
    std::streamsize xsputn(const char* data, std::streamsize size) override {
        if (std::string_view(data, static_cast<std::size_t>(size)).find(refused) != std::string_view::npos) {
            rejected = true;
            return 0;
        }
        return size;
    }
    int_type overflow(int_type value) override {
        return traits_type::not_eof(value);
    }
    std::string_view refused;
};

int checkReportingFailure(std::string_view refused) {
    WorldRepository repository;
    GameWorldInfoManager worlds;
    worlds.load(repository);
    const auto* previous = worlds.getGameWorldInfo(1);
    repository.worlds = {{3, "replacement", WORLD_OPEN}, {4, "another world", WORLD_CLOSE}};
    RefusingBuffer buffer(refused);
    auto* previousBuffer = std::cout.rdbuf(&buffer);
    const auto previousExceptions = std::cout.exceptions();
    std::cout.exceptions(std::ios::badbit | std::ios::failbit);
    bool failed = false;
    try {
        worlds.load(repository);
    } catch (const std::ios_base::failure&) {
        failed = true;
    }
    std::cout.exceptions(std::ios::goodbit);
    std::cout.rdbuf(previousBuffer);
    std::cout.clear();
    std::cout.exceptions(previousExceptions);
    if (!failed || !buffer.rejected || worlds.getSize() != 2 || worlds.getGameWorldInfo(1) != previous ||
        worlds.getGameWorldInfo(1)->getName() != "first world")
        return 1;
    try {
        worlds.getGameWorldInfo(3);
        return 2;
    } catch (const NoSuchElementException&) {
    }
    worlds.load(repository);
    return worlds.getGameWorldInfo(3)->getName() == "replacement" ? 0 : 3;
}

TEST(WorldCatalogue, ThrowingLoadDiagnosticsRetainPreviousRowsAndAllowRetry) {
    for (const char* refused :
         {"Loading GameWorldInfoManager", "GameWorldInfo(WorldID: 4", "Size : ", "End GameWorldInfoManager Load"}) {
        SCOPED_TRACE(refused);
        ASSERT_EXIT(std::_Exit(checkReportingFailure(refused)), ::testing::ExitedWithCode(0), "");
    }
}

} // namespace
