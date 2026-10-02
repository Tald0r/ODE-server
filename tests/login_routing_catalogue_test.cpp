#include <array>
#include <cstdlib>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <type_traits>

#include "DatabaseError.h"
#include "ZoneGroupInfoManager.h"
#include "ZoneInfoManager.h"
#include "repository/LoginConfigRepository.h"
#include "support/AllocationProbe.h"

namespace {

class RoutingRepository : public LoginConfigRepository {
public:
    std::vector<LoginZoneRow> loadZones() override {
        ++zoneReads;
        if (failure)
            std::rethrow_exception(failure);
        return zones;
    }
    std::vector<LoginZoneGroupRow> loadZoneGroups() override {
        ++groupReads;
        if (failure)
            std::rethrow_exception(failure);
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

    unsigned zoneReads = 0;
    unsigned groupReads = 0;
    std::exception_ptr failure;
    std::vector<LoginZoneRow> zones = {{10, 3}, {20, 4}};
    std::vector<LoginZoneGroupRow> groups = {{3, 7}, {4, 9}};
};

TEST(LoginRoutingCatalogue, UnloadedManagersHaveSafeEmptyLifetimesAndMissingLookups) {
    ZoneInfoManager zones;
    ZoneGroupInfoManager groups;
    EXPECT_EQ(zones.getSize(), 0u);
    EXPECT_EQ(groups.getSize(), 0u);
    EXPECT_THROW(zones.getZoneInfo(0), NoSuchElementException);
    EXPECT_THROW(groups.getZoneGroupInfo(65535), NoSuchElementException);
}

TEST(LoginRoutingCatalogue, SuppliedRowsKeepMappingsAndTheEntireStoredWordWidths) {
    RoutingRepository repository;
    repository.zones = {{0, 65535}, {65535, 0}, {10, 3}};
    repository.groups = {{0, 65535}, {65535, 0}, {3, 7}};
    ZoneInfoManager zones;
    ZoneGroupInfoManager groups;
    zones.load(repository);
    groups.load(repository);
    EXPECT_EQ(zones.getSize(), 3u);
    EXPECT_EQ(groups.getSize(), 3u);
    EXPECT_EQ(zones.getZoneInfo(0)->getZoneGroupID(), 65535);
    EXPECT_EQ(zones.getZoneInfo(65535)->getZoneID(), 65535);
    EXPECT_EQ(zones.getZoneInfo(65535)->getZoneGroupID(), 0);
    EXPECT_EQ(groups.getZoneGroupInfo(0)->getServerID(), 65535);
    EXPECT_EQ(groups.getZoneGroupInfo(65535)->getZoneGroupID(), 65535);
    EXPECT_EQ(groups.getZoneGroupInfo(65535)->getServerID(), 0);
    EXPECT_EQ(groups.getZoneGroupInfo(zones.getZoneInfo(10)->getZoneGroupID())->getServerID(), 7);
    EXPECT_EQ(repository.zoneReads, 1u);
    EXPECT_EQ(repository.groupReads, 1u);
}

TEST(LoginRoutingCatalogue, ZoneReloadReplacesChangedRowsAndRemovesStaleRoutes) {
    RoutingRepository repository;
    ZoneInfoManager zones;
    zones.load(repository);
    repository.zones = {{10, 8}, {30, 9}};
    zones.load(repository);
    EXPECT_EQ(zones.getSize(), 2u);
    EXPECT_EQ(zones.getZoneInfo(10)->getZoneGroupID(), 8);
    EXPECT_EQ(zones.getZoneInfo(30)->getZoneGroupID(), 9);
    EXPECT_THROW(zones.getZoneInfo(20), NoSuchElementException);
}

TEST(LoginRoutingCatalogue, GroupReloadReplacesChangedRowsAndRemovesStaleRoutes) {
    RoutingRepository repository;
    ZoneGroupInfoManager groups;
    groups.load(repository);
    repository.groups = {{3, 77}, {6, 8}};
    groups.load(repository);
    EXPECT_EQ(groups.getSize(), 2u);
    EXPECT_EQ(groups.getZoneGroupInfo(3)->getServerID(), 77);
    EXPECT_EQ(groups.getZoneGroupInfo(6)->getServerID(), 8);
    EXPECT_THROW(groups.getZoneGroupInfo(4), NoSuchElementException);
}

TEST(LoginRoutingCatalogue, EmptyReloadsRemoveEveryPreviousRow) {
    RoutingRepository repository;
    ZoneInfoManager zones;
    ZoneGroupInfoManager groups;
    zones.load(repository);
    groups.load(repository);
    repository.zones.clear();
    repository.groups.clear();
    zones.load(repository);
    groups.load(repository);
    EXPECT_EQ(zones.getSize(), 0u);
    EXPECT_EQ(groups.getSize(), 0u);
    EXPECT_THROW(zones.getZoneInfo(10), NoSuchElementException);
    EXPECT_THROW(groups.getZoneGroupInfo(3), NoSuchElementException);
}

TEST(LoginRoutingCatalogue, DuplicateZonesRefuseTheWholeLoadAndPreserveBorrowedRows) {
    RoutingRepository repository;
    ZoneInfoManager zones;
    zones.load(repository);
    const auto* previous = zones.getZoneInfo(10);
    repository.zones = {{30, 8}, {30, 9}};
    EXPECT_THROW(zones.load(repository), DuplicatedException);
    EXPECT_EQ(zones.getSize(), 2u);
    EXPECT_EQ(zones.getZoneInfo(10), previous);
    EXPECT_EQ(previous->getZoneGroupID(), 3);
    EXPECT_THROW(zones.getZoneInfo(30), NoSuchElementException);
}

TEST(LoginRoutingCatalogue, DuplicateGroupsRefuseTheWholeLoadAndPreserveBorrowedRows) {
    RoutingRepository repository;
    ZoneGroupInfoManager groups;
    groups.load(repository);
    const auto* previous = groups.getZoneGroupInfo(3);
    repository.groups = {{6, 10}, {6, 11}};
    EXPECT_THROW(groups.load(repository), DuplicatedException);
    EXPECT_EQ(groups.getSize(), 2u);
    EXPECT_EQ(groups.getZoneGroupInfo(3), previous);
    EXPECT_EQ(previous->getServerID(), 7);
    EXPECT_THROW(groups.getZoneGroupInfo(6), NoSuchElementException);
}

const ZoneInfo* lookup(ZoneInfoManager& zones, WORD id) {
    return zones.getZoneInfo(id);
}

const ZoneGroupInfo* lookup(ZoneGroupInfoManager& groups, WORD id) {
    return groups.getZoneGroupInfo(id);
}

template <class Manager> int checkAllocationFailure(std::size_t failAt, bool populated) {
    RoutingRepository repository;
    auto manager = std::make_unique<Manager>();
    if (populated)
        manager->load(repository);
    const WORD previousID = std::is_same_v<Manager, ZoneInfoManager> ? 10 : 3;
    const WORD nextID = std::is_same_v<Manager, ZoneInfoManager> ? 30 : 6;
    const auto* previous = populated ? lookup(*manager, previousID) : nullptr;
    repository.zones = {{30, 8}, {40, 9}};
    repository.groups = {{6, 10}, {7, 11}};
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        manager->load(repository);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 32 && failed))
        return 1;
    if (failed && (manager->getSize() != (populated ? 2u : 0u) || probe.outstanding() != 0 ||
                   (populated && lookup(*manager, previousID) != previous)))
        return 2;
    manager->load(repository);
    if (manager->getSize() != 2 || !lookup(*manager, nextID))
        return 3;
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 4;
}

TEST(LoginRoutingCatalogue, AllocationFailuresPreserveEmptyAndPopulatedTablesAndAllowRetry) {
    for (const bool populated : {false, true}) {
        for (std::size_t failAt = 1; failAt <= 32; ++failAt) {
            SCOPED_TRACE(populated);
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure<ZoneInfoManager>(failAt, populated)),
                        ::testing::ExitedWithCode(0), "");
            ASSERT_EXIT(std::_Exit(checkAllocationFailure<ZoneGroupInfoManager>(failAt, populated)),
                        ::testing::ExitedWithCode(0), "");
        }
    }
}

template <class Manager> int checkRepositoryFailure(unsigned kind, bool populated) {
    RoutingRepository repository;
    auto manager = std::make_unique<Manager>();
    if (populated)
        manager->load(repository);
    const WORD previousID = std::is_same_v<Manager, ZoneInfoManager> ? 10 : 3;
    const auto* previous = populated ? lookup(*manager, previousID) : nullptr;
    const std::array failures{std::make_exception_ptr(std::runtime_error("lookup failed")),
                              std::make_exception_ptr(Error("lookup failed")),
                              std::make_exception_ptr(DatabaseError("lookup failed"))};
    repository.failure = failures[kind];
    AllocationProbe probe;
    std::exception_ptr caught;
    bool translated = false;
    try {
        manager->load(repository);
    } catch (const Error& error) {
        caught = std::current_exception();
        const char* expected = std::is_same_v<Manager, ZoneInfoManager> ? "ZoneInfoManager::load : lookup failed"
                                                                        : "ZoneGroupInfoManager::load : lookup failed";
        translated = error.toString().find(expected) != std::string::npos;
    } catch (...) {
        caught = std::current_exception();
    }
    if (kind == 2 ? !translated : caught != repository.failure)
        return 1;
    caught = nullptr;
    if (manager->getSize() != (populated ? 2u : 0u) || (populated && lookup(*manager, previousID) != previous) ||
        probe.outstanding() != 0)
        return 2;
    repository.failure = nullptr;
    manager->load(repository);
    if (manager->getSize() != 2 || !lookup(*manager, previousID))
        return 3;
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 4;
}

TEST(LoginRoutingCatalogue, RepositoryErrorsKeepTranslationBorrowedRowsCleanupAndRetry) {
    for (const bool populated : {false, true}) {
        for (unsigned kind = 0; kind < 3; ++kind) {
            SCOPED_TRACE(populated);
            SCOPED_TRACE(kind);
            ASSERT_EXIT(std::_Exit(checkRepositoryFailure<ZoneInfoManager>(kind, populated)),
                        ::testing::ExitedWithCode(0), "");
            ASSERT_EXIT(std::_Exit(checkRepositoryFailure<ZoneGroupInfoManager>(kind, populated)),
                        ::testing::ExitedWithCode(0), "");
        }
    }
}

template <class Manager> int checkDuplicateFailure(bool populated) {
    RoutingRepository repository;
    auto manager = std::make_unique<Manager>();
    if (populated)
        manager->load(repository);
    const WORD previousID = std::is_same_v<Manager, ZoneInfoManager> ? 10 : 3;
    const WORD nextID = std::is_same_v<Manager, ZoneInfoManager> ? 30 : 6;
    const auto* previous = populated ? lookup(*manager, previousID) : nullptr;
    repository.zones = {{30, 8}, {40, 9}, {30, 10}};
    repository.groups = {{6, 10}, {7, 11}, {6, 12}};
    AllocationProbe probe;
    bool refused = false;
    try {
        manager->load(repository);
    } catch (const DuplicatedException& error) {
        refused = error.toString().find("duplicated zone id") != std::string::npos;
    }
    if (!refused || manager->getSize() != (populated ? 2u : 0u) ||
        (populated && lookup(*manager, previousID) != previous) || probe.outstanding() != 0)
        return 1;
    repository.zones.pop_back();
    repository.groups.pop_back();
    manager->load(repository);
    if (manager->getSize() != 2 || !lookup(*manager, nextID))
        return 2;
    manager.reset();
    return probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginRoutingCatalogue, DuplicateRefusalReleasesAllPreparedRowsOnEmptyAndPopulatedTablesAndCanRetry) {
    for (const bool populated : {false, true}) {
        SCOPED_TRACE(populated);
        ASSERT_EXIT(std::_Exit(checkDuplicateFailure<ZoneInfoManager>(populated)), ::testing::ExitedWithCode(0), "");
        ASSERT_EXIT(std::_Exit(checkDuplicateFailure<ZoneGroupInfoManager>(populated)), ::testing::ExitedWithCode(0),
                    "");
    }
}

int checkRepeatedLifetime() {
    RoutingRepository original;
    RoutingRepository replacement;
    replacement.zones = {{0, 65535}, {65535, 0}};
    replacement.groups = {{0, 65535}, {65535, 0}};
    RoutingRepository empty;
    empty.zones.clear();
    empty.groups.clear();
    AllocationProbe probe;
    for (unsigned repeat = 0; repeat < 8; ++repeat) {
        {
            ZoneInfoManager zones;
            ZoneGroupInfoManager groups;
            zones.load(original);
            groups.load(original);
            zones.load(replacement);
            groups.load(replacement);
            zones.load(empty);
            groups.load(empty);
            if (zones.getSize() != 0 || groups.getSize() != 0 || probe.outstanding() != 0)
                return 1;
            zones.load(replacement);
            groups.load(replacement);
        }
        if (probe.outstanding() != 0)
            return 2;
    }
    return 0;
}

TEST(LoginRoutingCatalogue, RepeatedReplacementEmptyLoadsAndDestructionReleaseAllOwnedStorage) {
    ASSERT_EXIT(std::_Exit(checkRepeatedLifetime()), ::testing::ExitedWithCode(0), "");
}

TEST(LoginRoutingCatalogue, ConstViewsExposeImmutableBorrowedRows) {
    RoutingRepository repository;
    ZoneInfoManager zones;
    ZoneGroupInfoManager groups;
    static_assert(!std::is_copy_constructible_v<ZoneInfoManager>);
    static_assert(!std::is_copy_constructible_v<ZoneGroupInfoManager>);
    static_assert(std::is_same_v<decltype(std::as_const(zones).getZoneInfo(0)), const ZoneInfo*>);
    static_assert(std::is_same_v<decltype(std::as_const(groups).getZoneGroupInfo(0)), const ZoneGroupInfo*>);
    zones.load(repository);
    groups.load(repository);
    EXPECT_EQ(std::as_const(zones).getZoneInfo(10)->getZoneGroupID(), 3);
    EXPECT_EQ(std::as_const(groups).getZoneGroupInfo(3)->getServerID(), 7);
}

TEST(LoginRoutingCatalogue, DiagnosticTextKeepsEmptyAndMissingRowIdentifiers) {
    ZoneInfoManager zones;
    ZoneGroupInfoManager groups;
    EXPECT_EQ(zones.toString(), "ZoneInfoManager(\nEMPTY)");
    EXPECT_EQ(groups.toString(), "ZoneGroupInfoManager(EMPTY)");
    try {
        zones.getZoneInfo(65535);
        FAIL() << "Missing zone accepted";
    } catch (const NoSuchElementException& error) {
        EXPECT_NE(error.toString().find("ZoneID : 65535"), std::string::npos);
    }
    try {
        groups.getZoneGroupInfo(65535);
        FAIL() << "Missing group accepted";
    } catch (const NoSuchElementException& error) {
        EXPECT_NE(error.toString().find("ZoneGroupID : 65535"), std::string::npos);
    }
    RoutingRepository repository;
    zones.load(repository);
    groups.load(repository);
    EXPECT_NE(zones.toString().find("ZoneInfo(ZoneID:10,ZoneGroupID:3)"), std::string::npos);
    EXPECT_NE(groups.toString().find("ZoneGroupInfo(ZoneGroupID:3,ServerID:7)"), std::string::npos);
}

} // namespace
