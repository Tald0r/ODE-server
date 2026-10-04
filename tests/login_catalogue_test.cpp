#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <string_view>
#include <type_traits>

#include "DatabaseError.h"
#include "GameServerGroupInfoManager.h"
#include "UserInfoManager.h"
#include "repository/LoginConfigRepository.h"
#include "support/AllocationProbe.h"

namespace {

class CatalogueRepository : public LoginConfigRepository {
public:
    bool loadMaxGameServerGroupWorldID(int& world) override {
        if (maximumFailure)
            std::rethrow_exception(maximumFailure);
        world = maximum;
        return hasMaximum;
    }
    std::vector<LoginGameServerGroupRow> loadGameServerGroups() override {
        ++groupReads;
        if (rowsFailure)
            std::rethrow_exception(rowsFailure);
        return groups;
    }
    std::vector<LoginGameServerGroupIDRow> loadGameServerGroupIDs() override {
        ++populationReads;
        if (rowsFailure)
            std::rethrow_exception(rowsFailure);
        return populations;
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

    int maximum = 2;
    bool hasMaximum = true;
    unsigned groupReads = 0;
    unsigned populationReads = 0;
    std::exception_ptr maximumFailure;
    std::exception_ptr rowsFailure;
    std::vector<LoginGameServerGroupRow> groups = {
        {1, 0, "First group", SERVER_FREE}, {1, 1, "Second group", SERVER_DOWN}, {2, 0, "Other world", SERVER_NORMAL}};
    std::vector<LoginGameServerGroupIDRow> populations = {{1, 0}, {1, 1}, {2, 0}};
};

template <typename Manager> void checkUnloadedDestruction() {
    alignas(Manager) unsigned char storage[sizeof(Manager)];
    std::fill_n(storage, sizeof(storage), 0xA5);
    auto* manager = ::new (static_cast<void*>(storage)) Manager;
    std::destroy_at(manager);
    std::_Exit(0);
}

TEST(LoginCatalogue, UnloadedGroupManagerHasASafeDestructor) {
    ASSERT_EXIT(checkUnloadedDestruction<GameServerGroupInfoManager>(), ::testing::ExitedWithCode(0), "");
}

TEST(LoginCatalogue, UnloadedPopulationManagerHasASafeDestructor) {
    ASSERT_EXIT(checkUnloadedDestruction<UserInfoManager>(), ::testing::ExitedWithCode(0), "");
}

TEST(LoginCatalogue, LoadsGroupFieldsFromTheSuppliedRepository) {
    CatalogueRepository repository;
    GameServerGroupInfoManager groups;
    groups.load(repository);
    EXPECT_EQ(groups.getSize(1), 2u);
    EXPECT_EQ(groups.getSize(2), 1u);
    const auto* first = groups.getGameServerGroupInfo(0, 1);
    EXPECT_EQ(first->getWorldID(), 1);
    EXPECT_EQ(first->getGroupID(), 0);
    EXPECT_EQ(first->getGroupName(), "First group");
    EXPECT_EQ(groups.getGameServerGroupInfo(1, 1)->getStat(), SERVER_DOWN);
    EXPECT_EQ(groups.getGameServerGroupInfo(0, 2)->getGroupName(), "Other world");
    EXPECT_EQ(repository.groupReads, 1u);
    EXPECT_EQ(repository.populationReads, 0u);
}

TEST(LoginCatalogue, LoadsIndependentPopulationCountersAtZeroAndKeepsThemMutable) {
    CatalogueRepository repository;
    UserInfoManager users;
    users.load(repository);
    auto* first = users.getUserInfo(0, 1);
    EXPECT_EQ(first->getWorldID(), 1);
    EXPECT_EQ(first->getServerGroupID(), 0);
    EXPECT_EQ(first->getUserNum(), 0);
    first->setUserNum(123);
    EXPECT_EQ(users.getUserInfo(0, 1)->getUserNum(), 123);
    EXPECT_EQ(users.getUserInfo(0, 2)->getUserNum(), 0);
    EXPECT_EQ(users.getSize(1), 2u);
    EXPECT_EQ(repository.groupReads, 0u);
    EXPECT_EQ(repository.populationReads, 1u);
}

TEST(LoginCatalogue, EmptyMaximumRefusalPreservesPreviouslyLoadedRows) {
    CatalogueRepository repository;
    GameServerGroupInfoManager groups;
    UserInfoManager users;
    groups.load(repository);
    users.load(repository);
    const auto* group = groups.getGameServerGroupInfo(0, 1);
    const auto* user = users.getUserInfo(0, 1);
    repository.hasMaximum = false;
    EXPECT_THROW(groups.load(repository), Error);
    EXPECT_THROW(users.load(repository), Error);
    EXPECT_EQ(groups.getGameServerGroupInfo(0, 1), group);
    EXPECT_EQ(users.getUserInfo(0, 1), user);
    EXPECT_EQ(repository.groupReads, 1u);
    EXPECT_EQ(repository.populationReads, 1u);
}

TEST(LoginCatalogue, FailedGroupFetchRetainsThePreviousCatalogueAndBorrowedRows) {
    CatalogueRepository repository;
    GameServerGroupInfoManager groups;
    groups.load(repository);
    const auto* previous = groups.getGameServerGroupInfo(0, 1);
    repository.rowsFailure = std::make_exception_ptr(DatabaseError("group fetch failed"));
    EXPECT_THROW(groups.load(repository), Error);
    ASSERT_EQ(groups.getSize(1), 2u);
    EXPECT_EQ(groups.getGameServerGroupInfo(0, 1), previous);
}

TEST(LoginCatalogue, FailedPopulationFetchRetainsBorrowedRowsAndLiveCounts) {
    CatalogueRepository repository;
    UserInfoManager users;
    users.load(repository);
    auto* previous = users.getUserInfo(0, 1);
    previous->setUserNum(123);
    repository.rowsFailure = std::make_exception_ptr(DatabaseError("population fetch failed"));
    EXPECT_THROW(users.load(repository), Error);
    ASSERT_EQ(users.getSize(1), 2u);
    ASSERT_EQ(users.getUserInfo(0, 1), previous);
    EXPECT_EQ(previous->getUserNum(), 123);
}

TEST(LoginCatalogue, DuplicateGroupRowsRefuseTheWholeReplacement) {
    CatalogueRepository repository;
    GameServerGroupInfoManager groups;
    groups.load(repository);
    const auto* previous = groups.getGameServerGroupInfo(0, 1);
    repository.groups.push_back(repository.groups.front());
    EXPECT_THROW(groups.load(repository), DuplicatedException);
    EXPECT_EQ(groups.getGameServerGroupInfo(0, 1), previous);
}

TEST(LoginCatalogue, DuplicatePopulationRowsCannotReplaceExistingCounts) {
    CatalogueRepository repository;
    UserInfoManager users;
    users.load(repository);
    auto* previous = users.getUserInfo(0, 1);
    previous->setUserNum(123);
    repository.populations.push_back(repository.populations.front());
    EXPECT_THROW(users.load(repository), DuplicatedException);
    ASSERT_EQ(users.getUserInfo(0, 1), previous);
    EXPECT_EQ(previous->getUserNum(), 123);
}

template <typename Manager> int checkWorldZeroCleanup() {
    CatalogueRepository repository;
    {
        Manager warmup;
        warmup.load(repository);
    }
    repository.maximum = 0;
    repository.groups = {{0, 0, "World zero", SERVER_FREE}};
    repository.populations = {{0, 0}};
    AllocationProbe probe;
    {
        Manager manager;
        manager.load(repository);
        if (manager.getSize(0) != 1)
            return 1;
    }
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginCatalogue, DestructionReleasesWorldZeroGroupRows) {
    ASSERT_EXIT(std::_Exit(checkWorldZeroCleanup<GameServerGroupInfoManager>()), ::testing::ExitedWithCode(0), "");
}

TEST(LoginCatalogue, DestructionReleasesWorldZeroPopulationRows) {
    ASSERT_EXIT(std::_Exit(checkWorldZeroCleanup<UserInfoManager>()), ::testing::ExitedWithCode(0), "");
}

int checkBoundaryWorld(int worldID) {
    CatalogueRepository repository;
    repository.maximum = worldID;
    repository.groups = {{worldID, 255, "Last group", SERVER_FREE}};
    repository.populations = {{worldID, 255}};
    GameServerGroupInfoManager groups;
    UserInfoManager users;
    groups.load(repository);
    users.load(repository);
    return groups.getSize(worldID) == 1 && users.getSize(worldID) == 1 &&
                   groups.getGameServerGroupInfo(255, worldID)->getWorldID() == worldID &&
                   users.getUserInfo(255, worldID)->getServerGroupID() == 255
               ? 0
               : 1;
}

TEST(LoginCatalogue, MaximumWorldIDsDoNotWrapTheAllocatedDimension) {
    for (const int worldID : {254, 255}) {
        SCOPED_TRACE(worldID);
        ASSERT_EXIT(std::_Exit(checkBoundaryWorld(worldID)), ::testing::ExitedWithCode(0), "");
    }
}

template <typename Manager> auto* firstRow(Manager& manager) {
    if constexpr (std::is_same_v<Manager, GameServerGroupInfoManager>)
        return manager.getGameServerGroupInfo(0, 1);
    else
        return manager.getUserInfo(0, 1);
}

template <typename Manager> void setLiveCount(Manager& manager) {
    if constexpr (std::is_same_v<Manager, UserInfoManager>)
        firstRow(manager)->setUserNum(123);
}

template <typename Manager, typename Row> bool retained(Manager& manager, const Row* previous) {
    if (manager.getSize(1) != 2 || manager.getSize(2) != 1 || firstRow(manager) != previous)
        return false;
    if constexpr (std::is_same_v<Manager, GameServerGroupInfoManager>)
        return previous->getGroupName() == "First group";
    else
        return previous->getUserNum() == 123;
}

CatalogueRepository replacementRepository() {
    CatalogueRepository replacement;
    replacement.maximum = 255;
    replacement.groups = {{0, 0, "first replacement " + std::string(96, 'a'), SERVER_FREE},
                          {255, 255, "last replacement " + std::string(128, 'b'), SERVER_DOWN}};
    replacement.populations = {{0, 0}, {255, 255}};
    return replacement;
}

TEST(LoginCatalogue, UnloadedCountsAndMissingLookupsAreBounded) {
    GameServerGroupInfoManager groups;
    UserInfoManager users;
    for (const WorldID_t world : {0, 1, 255}) {
        EXPECT_EQ(groups.getSize(world), 0u);
        EXPECT_EQ(users.getSize(world), 0u);
        EXPECT_THROW(groups.getGameServerGroupInfo(0, world), NoSuchElementException);
        EXPECT_THROW(users.getUserInfo(0, world), NoSuchElementException);
    }
}

TEST(LoginCatalogue, SuccessfulReloadRemovesOldRowsAndStartsReplacementCountersAtZero) {
    CatalogueRepository original;
    auto replacement = replacementRepository();
    GameServerGroupInfoManager groups;
    UserInfoManager users;
    groups.load(original);
    users.load(original);
    users.getUserInfo(0, 1)->setUserNum(123);
    groups.load(replacement);
    users.load(replacement);
    EXPECT_EQ(groups.getSize(1), 0u);
    EXPECT_EQ(users.getSize(1), 0u);
    EXPECT_THROW(groups.getGameServerGroupInfo(0, 1), NoSuchElementException);
    EXPECT_THROW(users.getUserInfo(0, 1), NoSuchElementException);
    EXPECT_EQ(groups.getGameServerGroupInfo(0, 0)->getGroupName(), replacement.groups[0].groupName);
    EXPECT_EQ(groups.getGameServerGroupInfo(255, 255)->getStat(), SERVER_DOWN);
    EXPECT_EQ(users.getUserInfo(0, 0)->getUserNum(), 0);
    EXPECT_EQ(users.getUserInfo(255, 255)->getUserNum(), 0);
}

template <typename Manager> int checkAllocationFailure(std::size_t failAt, bool populated) {
    CatalogueRepository original;
    auto replacement = replacementRepository();
    {
        Manager warmup;
        warmup.load(original);
    }
    auto manager = std::make_unique<Manager>();
    if (populated) {
        manager->load(original);
        setLiveCount(*manager);
    }
    const auto* previous = populated ? firstRow(*manager) : nullptr;
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        manager->load(replacement);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 128 && failed))
        return 1;
    if (failed) {
        if (populated ? !retained(*manager, previous) : manager->getSize(1) != 0)
            return 2;
        if (manager->getSize(0) != 0 || manager->getSize(255) != 0 || probe.outstanding() != 0)
            return 3;
        manager->load(replacement);
    }
    if (manager->getSize(1) != 0 || manager->getSize(0) != 1 || manager->getSize(255) != 1)
        return 4;
    if constexpr (std::is_same_v<Manager, GameServerGroupInfoManager>) {
        if (manager->getGameServerGroupInfo(0, 0)->getGroupName() != replacement.groups[0].groupName ||
            manager->getGameServerGroupInfo(255, 255)->getStat() != SERVER_DOWN)
            return 5;
    } else if (manager->getUserInfo(0, 0)->getUserNum() != 0 || manager->getUserInfo(255, 255)->getUserNum() != 0)
        return 6;
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 7;
}

TEST(LoginCatalogue, AllocationFailuresPreserveEmptyAndPopulatedTablesReleasePreparationAndPermitRetry) {
    for (const bool populated : {false, true}) {
        SCOPED_TRACE(populated);
        for (std::size_t failAt = 1; failAt <= 128; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure<GameServerGroupInfoManager>(failAt, populated)),
                        ::testing::ExitedWithCode(0), "");
            ASSERT_EXIT(std::_Exit(checkAllocationFailure<UserInfoManager>(failAt, populated)),
                        ::testing::ExitedWithCode(0), "");
        }
    }
}

template <typename Manager> int checkInvalidRow(unsigned field, int value) {
    CatalogueRepository original;
    Manager manager;
    manager.load(original);
    setLiveCount(manager);
    const auto* previous = firstRow(manager);
    CatalogueRepository invalid;
    if (field == 0)
        invalid.maximum = value;
    else if constexpr (std::is_same_v<Manager, GameServerGroupInfoManager>) {
        if (field == 1)
            invalid.groups[1].worldID = value;
        if (field == 2)
            invalid.groups[1].groupID = value;
        if (field == 3)
            invalid.groups[1].stat = value;
    } else {
        if (field == 1)
            invalid.populations[1].worldID = value;
        if (field == 2)
            invalid.populations[1].groupID = value;
    }
    bool refused = false;
    try {
        manager.load(invalid);
    } catch (const Error&) {
        refused = true;
    }
    if (!refused || !retained(manager, previous))
        return 1;
    manager.load(original);
    return manager.getSize(1) == 2 ? 0 : 2;
}

TEST(LoginCatalogue, InvalidDimensionsAndFieldsCannotNarrowIndexOutsideStorageOrReplaceRows) {
    for (const int maximum : {-1, 256, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        SCOPED_TRACE(maximum);
        ASSERT_EXIT(std::_Exit(checkInvalidRow<GameServerGroupInfoManager>(0, maximum)), ::testing::ExitedWithCode(0),
                    "");
        ASSERT_EXIT(std::_Exit(checkInvalidRow<UserInfoManager>(0, maximum)), ::testing::ExitedWithCode(0), "");
    }
    for (const int world : {-1, 256, 3}) {
        SCOPED_TRACE(world);
        ASSERT_EXIT(std::_Exit(checkInvalidRow<GameServerGroupInfoManager>(1, world)), ::testing::ExitedWithCode(0),
                    "");
        ASSERT_EXIT(std::_Exit(checkInvalidRow<UserInfoManager>(1, world)), ::testing::ExitedWithCode(0), "");
    }
    for (const int value : {-1, 256}) {
        SCOPED_TRACE(value);
        ASSERT_EXIT(std::_Exit(checkInvalidRow<GameServerGroupInfoManager>(2, value)), ::testing::ExitedWithCode(0),
                    "");
        ASSERT_EXIT(std::_Exit(checkInvalidRow<GameServerGroupInfoManager>(3, value)), ::testing::ExitedWithCode(0),
                    "");
    }
    for (const int group : {-1, 65536}) {
        SCOPED_TRACE(group);
        ASSERT_EXIT(std::_Exit(checkInvalidRow<UserInfoManager>(2, group)), ::testing::ExitedWithCode(0), "");
    }
}

template <typename Manager> void checkRepositoryErrors() {
    const std::array failures{std::make_exception_ptr(std::runtime_error("probe failed")),
                              std::make_exception_ptr(Error("probe failed")),
                              std::make_exception_ptr(DatabaseError("probe failed"))};
    for (const bool rows : {false, true}) {
        SCOPED_TRACE(rows);
        for (unsigned kind = 0; kind < failures.size(); ++kind) {
            SCOPED_TRACE(kind);
            CatalogueRepository repository;
            Manager manager;
            manager.load(repository);
            setLiveCount(manager);
            const auto* previous = firstRow(manager);
            (rows ? repository.rowsFailure : repository.maximumFailure) = failures[kind];
            std::exception_ptr caught;
            bool translated = false;
            try {
                manager.load(repository);
            } catch (const Error& error) {
                caught = std::current_exception();
                const char* expected = std::is_same_v<Manager, GameServerGroupInfoManager>
                                           ? "GameServerGroupInfoManager::load : probe failed"
                                           : "UserInfoManager::load : probe failed";
                translated = error.toString().find(expected) != std::string::npos;
            } catch (...) {
                caught = std::current_exception();
            }
            if (rows && kind == 2)
                EXPECT_TRUE(translated);
            else
                EXPECT_EQ(caught, failures[kind]);
            EXPECT_TRUE(retained(manager, previous));
            repository.maximumFailure = nullptr;
            repository.rowsFailure = nullptr;
            EXPECT_NO_THROW(manager.load(repository));
        }
    }
}

TEST(LoginCatalogue, RepositoryFailuresPreserveTheirStageSpecificTranslationAndAllowRetry) {
    checkRepositoryErrors<GameServerGroupInfoManager>();
    checkRepositoryErrors<UserInfoManager>();
}

template <typename Manager> int checkDuplicateCleanup() {
    CatalogueRepository original;
    auto rejected = replacementRepository();
    rejected.groups.push_back(rejected.groups.front());
    rejected.populations.push_back(rejected.populations.front());
    auto manager = std::make_unique<Manager>();
    manager->load(original);
    setLiveCount(*manager);
    const auto* previous = firstRow(*manager);
    AllocationProbe probe;
    bool refused = false;
    try {
        manager->load(rejected);
    } catch (const DuplicatedException&) {
        refused = true;
    }
    if (!refused || !retained(*manager, previous))
        return 1;
    if (probe.outstanding() != 0)
        return 2;
    manager->load(original);
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginCatalogue, DuplicateRefusalReleasesPreparedRowsAndPreservesLiveCounters) {
    ASSERT_EXIT(std::_Exit(checkDuplicateCleanup<GameServerGroupInfoManager>()), ::testing::ExitedWithCode(0), "");
    ASSERT_EXIT(std::_Exit(checkDuplicateCleanup<UserInfoManager>()), ::testing::ExitedWithCode(0), "");
}

class RefusingBuffer : public std::streambuf {
public:
    explicit RefusingBuffer(std::string_view text) : text(text) {}
    bool rejected = false;

private:
    std::streamsize xsputn(const char* data, std::streamsize size) override {
        if (std::string_view(data, static_cast<std::size_t>(size)).find(text) != std::string_view::npos) {
            rejected = true;
            return 0;
        }
        return size;
    }
    int_type overflow(int_type value) override {
        return traits_type::not_eof(value);
    }
    std::string_view text;
};

int checkReportingFailure(std::string_view text) {
    ::setenv("DARKEDEN_TRACE", "1", 1);
    CatalogueRepository original;
    auto replacement = replacementRepository();
    GameServerGroupInfoManager groups;
    groups.load(original);
    RefusingBuffer buffer(text);
    auto* previousBuffer = std::cerr.rdbuf(&buffer);
    const auto previousExceptions = std::cerr.exceptions();
    std::cerr.exceptions(std::ios::badbit | std::ios::failbit);
    bool failed = false;
    try {
        groups.load(replacement);
    } catch (...) {
        failed = true;
    }
    const bool streamHealthy = std::cerr.good();
    std::cerr.exceptions(std::ios::goodbit);
    std::cerr.rdbuf(previousBuffer);
    std::cerr.clear();
    std::cerr.exceptions(previousExceptions);
    if (failed || !buffer.rejected || !streamHealthy || groups.getSize(0) != 1 || groups.getSize(255) != 1 ||
        groups.getSize(1) != 0 || groups.getSize(2) != 0)
        return 1;
    const auto* first = groups.getGameServerGroupInfo(0, 0);
    const auto* last = groups.getGameServerGroupInfo(255, 255);
    return first->getGroupName() == replacement.groups[0].groupName && first->getStat() == SERVER_FREE &&
                   last->getGroupName() == replacement.groups[1].groupName && last->getStat() == SERVER_DOWN
               ? 0
               : 2;
}

TEST(LoginCatalogue, FailedOptionalGroupDiagnosticsCannotPreventCompletePublication) {
    for (const auto text : {"addGameServerGroupInfo", "first replacement", "last replacement"}) {
        SCOPED_TRACE(text);
        ASSERT_EXIT(std::_Exit(checkReportingFailure(text)), ::testing::ExitedWithCode(0), "");
    }
}

int checkCatalogueOutput(bool trace) {
    if (trace)
        ::setenv("DARKEDEN_TRACE", "1", 1);
    else
        ::unsetenv("DARKEDEN_TRACE");
    CatalogueRepository repository;
    GameServerGroupInfoManager groups;
    std::ostringstream output, error;
    auto* oldOutput = std::cout.rdbuf(output.rdbuf());
    auto* oldError = std::cerr.rdbuf(error.rdbuf());
    bool failed = false;
    try {
        groups.load(repository);
    } catch (...) {
        failed = true;
    }
    std::cout.rdbuf(oldOutput);
    std::cerr.rdbuf(oldError);
    if (failed || groups.getSize(1) != 2 || groups.getSize(2) != 1 || !output.str().empty())
        return 1;
    if (!trace)
        return error.str().empty() ? 0 : 2;
    return error.str().find("[debug] addGameServerGroupInfo") != std::string::npos &&
                   error.str().find("First group") != std::string::npos &&
                   error.str().find("Other world") != std::string::npos
               ? 0
               : 3;
}

TEST(LoginCatalogue, LoadingRowsIsQuietByDefaultAndTraceMustBeExplicitlyEnabled) {
    ASSERT_EXIT(std::_Exit(checkCatalogueOutput(false)), ::testing::ExitedWithCode(0), "");
    ASSERT_EXIT(std::_Exit(checkCatalogueOutput(true)), ::testing::ExitedWithCode(0), "");
}

TEST(LoginCatalogue, StoredBoundaryFieldsAndReadOnlyViewsKeepTheirExistingWidths) {
    CatalogueRepository repository;
    repository.maximum = 255;
    repository.groups = {{0, 0, "Zero", 0}, {255, 255, std::string(96, 'g'), 255}};
    repository.populations = {{0, 0}, {255, 65535}};
    GameServerGroupInfoManager groups;
    UserInfoManager users;
    groups.load(repository);
    users.load(repository);
    const auto* last = groups.getGameServerGroupInfo(255, 255);
    EXPECT_EQ(last->getGroupName(), repository.groups[1].groupName);
    EXPECT_EQ(last->getStat(), 255);
    EXPECT_EQ(users.getUserInfo(65535, 255)->getServerGroupID(), 65535);
    users.getUserInfo(65535, 255)->setUserNum(123);
    const auto* view = std::as_const(users).getUserInfo(65535, 255);
    static_assert(std::is_const_v<std::remove_pointer_t<decltype(std::as_const(users).getUserInfo(0, 0))>>);
    static_assert(std::is_const_v<std::remove_pointer_t<decltype(groups.getGameServerGroupInfo(0, 0))>>);
    EXPECT_EQ(view->getUserNum(), 123);
}

template <typename Manager> int checkRepeatedLoads() {
    CatalogueRepository original;
    auto replacement = replacementRepository();
    CatalogueRepository empty;
    empty.groups.clear();
    empty.populations.clear();
    {
        Manager warmup;
        warmup.load(original);
    }
    AllocationProbe probe;
    {
        Manager manager;
        manager.load(original);
        manager.load(replacement);
        manager.load(replacement);
        manager.load(empty);
        if (manager.getSize(0) != 0 || manager.getSize(1) != 0 || manager.getSize(255) != 0)
            return 1;
    }
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginCatalogue, RepeatedReplacementEmptyRowsAndDestructionReleaseEveryAllocation) {
    ASSERT_EXIT(std::_Exit(checkRepeatedLoads<GameServerGroupInfoManager>()), ::testing::ExitedWithCode(0), "");
    ASSERT_EXIT(std::_Exit(checkRepeatedLoads<UserInfoManager>()), ::testing::ExitedWithCode(0), "");
}

} // namespace
