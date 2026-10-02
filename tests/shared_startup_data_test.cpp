#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "ResurrectLocationManager.h"
#include "StringPool.h"
#include "repository/SharedConfigRepository.h"
#include "support/AllocationProbe.h"

namespace {

class StartupDataRepository : public SharedConfigRepository {
public:
    bool loadMaxGameServerGroupWorldID(int&) override {
        std::abort();
    }
    std::vector<SharedGameServerGroupRow> loadGameServerGroups() override {
        std::abort();
    }
    bool loadMaxGameServerGroupID(int, int&) override {
        std::abort();
    }
    std::vector<SharedGameServerRow> loadGameServers() override {
        std::abort();
    }
    std::vector<SharedResurrectLocationRow> loadResurrectLocations() override {
        if (failure)
            std::rethrow_exception(failure);
        return locations;
    }
    std::vector<SharedStringRow> loadStrings() override {
        if (failure)
            std::rethrow_exception(failure);
        return strings;
    }

    std::exception_ptr failure;
    std::vector<SharedResurrectLocationRow> locations = {{10, 11, 12, 13, 14, 15, 16}, {20, 21, 22, 23, 24, 25, 26}};
    std::vector<SharedStringRow> strings = {{0, "first"}, {1, "second"}};
};

void expectPosition(const ZONE_COORD& position, ZoneID_t zone, ZoneCoord_t x, ZoneCoord_t y) {
    EXPECT_EQ(position.id, zone);
    EXPECT_EQ(position.x, x);
    EXPECT_EQ(position.y, y);
}

TEST(SharedStartupData, LoadsBothResurrectionPositionsWithoutStartup) {
    StartupDataRepository repository;
    ResurrectLocationManager locations;
    locations.load(repository);
    ZONE_COORD position;
    ASSERT_TRUE(locations.getSlayerPosition(10, position));
    expectPosition(position, 11, 12, 13);
    ASSERT_TRUE(locations.getVampirePosition(10, position));
    expectPosition(position, 14, 15, 16);
    ASSERT_TRUE(locations.getSlayerPosition(20, position));
    expectPosition(position, 21, 22, 23);
    ASSERT_TRUE(locations.getVampirePosition(20, position));
    expectPosition(position, 24, 25, 26);
}

TEST(SharedStartupData, LoadsExactStringContentsWithoutStartup) {
    StartupDataRepository repository;
    repository.strings = {{0, ""}, {1, "first\nsecond"}, {2, std::string("ab\0cd", 5)}};
    StringPool strings;
    strings.load(repository);
    EXPECT_EQ(strings.getString(0), "");
    EXPECT_EQ(strings.getString(1), "first\nsecond");
    EXPECT_EQ(strings.getString(2), repository.strings[2].text);
    EXPECT_STREQ(strings.c_str(1), "first\nsecond");
}

TEST(SharedStartupData, ReloadReplacesPairedLocationsAndDropsRemovedZones) {
    StartupDataRepository repository;
    ResurrectLocationManager locations;
    locations.load(repository);
    repository.locations = {{10, 31, 32, 33, 34, 35, 36}};
    ASSERT_NO_THROW(locations.load(repository));
    ZONE_COORD position;
    ASSERT_TRUE(locations.getSlayerPosition(10, position));
    expectPosition(position, 31, 32, 33);
    ASSERT_TRUE(locations.getVampirePosition(10, position));
    expectPosition(position, 34, 35, 36);
    EXPECT_FALSE(locations.getSlayerPosition(20, position));
    EXPECT_FALSE(locations.getVampirePosition(20, position));
}

TEST(SharedStartupData, DuplicateLocationsDoNotPublishARowPrefix) {
    StartupDataRepository repository;
    ResurrectLocationManager locations;
    locations.load(repository);
    repository.locations = {{30, 31, 32, 33, 34, 35, 36}, {30, 41, 42, 43, 44, 45, 46}};
    EXPECT_THROW(locations.load(repository), NoSuchElementException);
    ZONE_COORD position;
    EXPECT_FALSE(locations.getSlayerPosition(30, position));
    EXPECT_FALSE(locations.getVampirePosition(30, position));
    ASSERT_TRUE(locations.getSlayerPosition(10, position));
    expectPosition(position, 11, 12, 13);
    ASSERT_TRUE(locations.getVampirePosition(10, position));
    expectPosition(position, 14, 15, 16);
}

TEST(SharedStartupData, FailedStringFetchPreservesThePreviousStrings) {
    StartupDataRepository repository;
    StringPool strings;
    strings.load(repository);
    repository.failure = std::make_exception_ptr(std::runtime_error("fetch failed"));
    EXPECT_THROW(strings.load(repository), std::runtime_error);
    EXPECT_EQ(strings.getString(0), "first");
    EXPECT_EQ(strings.getString(1), "second");
}

TEST(SharedStartupData, DuplicateStringsDoNotReplaceThePreviousStringsWithAPrefix) {
    StartupDataRepository repository;
    StringPool strings;
    strings.load(repository);
    repository.strings = {{2, "replacement"}, {2, "duplicate"}};
    EXPECT_THROW(strings.load(repository), DuplicatedException);
    EXPECT_THROW(strings.getString(2), NoSuchElementException);
    EXPECT_EQ(strings.getString(0), "first");
    EXPECT_EQ(strings.getString(1), "second");
}

bool matches(const ZONE_COORD& position, ZoneID_t zone, ZoneCoord_t x, ZoneCoord_t y) {
    return position.id == zone && position.x == x && position.y == y;
}

int checkLocationAllocationFailure(std::size_t failAt, bool populated) {
    StartupDataRepository original;
    StartupDataRepository replacement;
    replacement.locations = {{10, 31, 32, 33, 34, 35, 36}, {30, 41, 42, 43, 44, 45, 46}};
    auto locations = std::make_unique<ResurrectLocationManager>();
    if (populated)
        locations->load(original);
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        locations->load(replacement);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed))
        return 1;
    ZONE_COORD position(91, 92, 93);
    if (failed) {
        if (locations->getSlayerPosition(10, position) != populated ||
            !matches(position, populated ? 11 : 91, populated ? 12 : 92, populated ? 13 : 93))
            return 2;
        position.set(91, 92, 93);
        if (locations->getVampirePosition(10, position) != populated ||
            !matches(position, populated ? 14 : 91, populated ? 15 : 92, populated ? 16 : 93) ||
            locations->getSlayerPosition(30, position) || locations->getVampirePosition(30, position) ||
            probe.outstanding() != 0)
            return 3;
        locations->load(replacement);
    }
    if (!locations->getSlayerPosition(10, position) || !matches(position, 31, 32, 33) ||
        !locations->getVampirePosition(10, position) || !matches(position, 34, 35, 36) ||
        !locations->getSlayerPosition(30, position) || !matches(position, 41, 42, 43) ||
        !locations->getVampirePosition(30, position) || !matches(position, 44, 45, 46) ||
        locations->getSlayerPosition(20, position) || locations->getVampirePosition(20, position))
        return 4;
    locations.reset();
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(SharedStartupData, LocationAllocationFailuresPreserveBothRacesAndPermitRetry) {
    for (const bool populated : {false, true}) {
        SCOPED_TRACE(populated);
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(std::_Exit(checkLocationAllocationFailure(failAt, populated)), ::testing::ExitedWithCode(0),
                        "");
        }
    }
}

int checkStringAllocationFailure(std::size_t failAt, bool populated) {
    StartupDataRepository original;
    original.strings[0].text = std::string(96, 'a');
    StartupDataRepository replacement;
    replacement.strings = {{0, std::string(128, 'b')}, {5, std::string(192, 'c')}};
    auto strings = std::make_unique<StringPool>();
    const char* previous = nullptr;
    if (populated) {
        strings->load(original);
        previous = strings->c_str(0);
    }
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        strings->load(replacement);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed))
        return 1;
    if (failed) {
        if (populated) {
            if (strings->c_str(0) != previous || std::strcmp(previous, original.strings[0].text.c_str()) != 0 ||
                strings->getString(1) != "second")
                return 2;
        } else {
            try {
                strings->getString(0);
                return 3;
            } catch (const NoSuchElementException&) {
            }
        }
        try {
            strings->getString(5);
            return 4;
        } catch (const NoSuchElementException&) {
        }
        if (probe.outstanding() != 0)
            return 5;
        strings->load(replacement);
    }
    if (strings->getString(0) != replacement.strings[0].text || strings->getString(5) != replacement.strings[1].text)
        return 6;
    try {
        strings->getString(1);
        return 7;
    } catch (const NoSuchElementException&) {
    }
    strings.reset();
    return probe.outstanding() == 0 ? 0 : 8;
}

TEST(SharedStartupData, StringAllocationFailuresPreserveContentsAndBorrowedPointersAndPermitRetry) {
    for (const bool populated : {false, true}) {
        SCOPED_TRACE(populated);
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(std::_Exit(checkStringAllocationFailure(failAt, populated)), ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(SharedStartupData, InvalidLocationFieldsAreRefusedBeforeNarrowingOrPublication) {
    StartupDataRepository original;
    ResurrectLocationManager locations;
    locations.load(original);
    const auto fields = {&SharedResurrectLocationRow::zoneID,        &SharedResurrectLocationRow::slayerZoneID,
                         &SharedResurrectLocationRow::slayerX,       &SharedResurrectLocationRow::slayerY,
                         &SharedResurrectLocationRow::vampireZoneID, &SharedResurrectLocationRow::vampireX,
                         &SharedResurrectLocationRow::vampireY};
    unsigned index = 0;
    for (const auto field : fields) {
        SCOPED_TRACE(index++);
        for (const int invalid : {-1, 65536, std::numeric_limits<int>::max()}) {
            SCOPED_TRACE(invalid);
            StartupDataRepository rejected;
            rejected.locations.back().*field = invalid;
            EXPECT_THROW(locations.load(rejected), Error);
            ZONE_COORD position;
            ASSERT_TRUE(locations.getSlayerPosition(10, position));
            expectPosition(position, 11, 12, 13);
            ASSERT_TRUE(locations.getVampirePosition(20, position));
            expectPosition(position, 24, 25, 26);
        }
    }
}

TEST(SharedStartupData, NegativeStringIDsAreRefusedWithoutPublication) {
    StartupDataRepository original;
    StringPool strings;
    strings.load(original);
    const char* previous = strings.c_str(0);
    for (const int invalid : {-1, std::numeric_limits<int>::min()}) {
        SCOPED_TRACE(invalid);
        StartupDataRepository rejected;
        rejected.strings.back().id = invalid;
        EXPECT_THROW(strings.load(rejected), Error);
        EXPECT_EQ(strings.c_str(0), previous);
        EXPECT_EQ(strings.getString(0), "first");
        EXPECT_EQ(strings.getString(1), "second");
        EXPECT_THROW(strings.getString(static_cast<uint>(invalid)), NoSuchElementException);
    }
}

TEST(SharedStartupData, ZeroAndMaximumCoordinatesAndStringIDsRemainValid) {
    StartupDataRepository repository;
    repository.locations = {{0, 65535, 0, 65535, 0, 65535, 0}, {65535, 0, 65535, 0, 65535, 0, 65535}};
    repository.strings = {{0, "zero"}, {std::numeric_limits<int>::max(), "maximum"}};
    ResurrectLocationManager locations;
    StringPool strings;
    locations.load(repository);
    strings.load(repository);
    ZONE_COORD position;
    ASSERT_TRUE(locations.getSlayerPosition(0, position));
    expectPosition(position, 65535, 0, 65535);
    ASSERT_TRUE(locations.getVampirePosition(0, position));
    expectPosition(position, 0, 65535, 0);
    ASSERT_TRUE(locations.getSlayerPosition(65535, position));
    expectPosition(position, 0, 65535, 0);
    ASSERT_TRUE(locations.getVampirePosition(65535, position));
    expectPosition(position, 65535, 0, 65535);
    EXPECT_EQ(strings.getString(0), "zero");
    EXPECT_EQ(strings.getString(std::numeric_limits<int>::max()), "maximum");
}

TEST(SharedStartupData, RepositoryExceptionsPreserveBothTablesAndTheOriginalException) {
    StartupDataRepository repository;
    ResurrectLocationManager locations;
    StringPool strings;
    locations.load(repository);
    strings.load(repository);
    const char* previous = strings.c_str(0);
    for (const auto& failure : {std::make_exception_ptr(std::runtime_error("repository failed")),
                                std::make_exception_ptr(DatabaseError("database failed")),
                                std::make_exception_ptr(Error("legacy failure"))}) {
        repository.failure = failure;
        try {
            locations.load(repository);
            FAIL() << "expected the repository failure";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        try {
            strings.load(repository);
            FAIL() << "expected the repository failure";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        ZONE_COORD position;
        ASSERT_TRUE(locations.getSlayerPosition(10, position));
        expectPosition(position, 11, 12, 13);
        ASSERT_TRUE(locations.getVampirePosition(10, position));
        expectPosition(position, 14, 15, 16);
        EXPECT_EQ(strings.c_str(0), previous);
        EXPECT_EQ(strings.getString(0), "first");
    }
}

TEST(SharedStartupData, EmptyResurrectionResultsAreRejectedWithoutLosingPriorLocations) {
    StartupDataRepository repository;
    ResurrectLocationManager locations;
    locations.load(repository);
    repository.locations.clear();
    EXPECT_THROW(locations.load(repository), Error);
    ZONE_COORD position;
    ASSERT_TRUE(locations.getSlayerPosition(10, position));
    expectPosition(position, 11, 12, 13);
    ASSERT_TRUE(locations.getVampirePosition(10, position));
    expectPosition(position, 14, 15, 16);
    ResurrectLocationManager empty;
    EXPECT_THROW(empty.load(repository), Error);
    EXPECT_FALSE(empty.getSlayerPosition(10, position));
    EXPECT_FALSE(empty.getVampirePosition(10, position));
    repository.locations = {{10, 31, 32, 33, 34, 35, 36}};
    locations.load(repository);
    ASSERT_TRUE(locations.getSlayerPosition(10, position));
    expectPosition(position, 31, 32, 33);
}

TEST(SharedStartupData, EmptyStringResultsReplaceAllPreviousStrings) {
    StartupDataRepository repository;
    StringPool strings;
    strings.load(repository);
    repository.strings.clear();
    EXPECT_NO_THROW(strings.load(repository));
    EXPECT_THROW(strings.getString(0), NoSuchElementException);
    EXPECT_THROW(strings.c_str(1), NoSuchElementException);
    repository.strings = {{0, "retry"}};
    strings.load(repository);
    EXPECT_EQ(strings.getString(0), "retry");
}

TEST(SharedStartupData, MissingLookupsPreserveOutputAndUseExistingRefusals) {
    for (const bool populated : {false, true}) {
        SCOPED_TRACE(populated);
        StartupDataRepository repository;
        ResurrectLocationManager locations;
        StringPool strings;
        if (populated) {
            locations.load(repository);
            strings.load(repository);
        }
        ZONE_COORD position(91, 92, 93);
        EXPECT_FALSE(locations.getSlayerPosition(99, position));
        expectPosition(position, 91, 92, 93);
        EXPECT_FALSE(locations.getVampirePosition(99, position));
        expectPosition(position, 91, 92, 93);
        const StringPool& view = strings;
        EXPECT_THROW(view.getString(99), NoSuchElementException);
        EXPECT_THROW(view.c_str(99), NoSuchElementException);
    }
}

TEST(SharedStartupData, RepeatedLoadsReleaseEveryPreviousAllocation) {
    ASSERT_EXIT(
        {
            StartupDataRepository repository;
            repository.strings.front().text = std::string(128, 'a');
            AllocationProbe probe;
            {
                ResurrectLocationManager locations;
                StringPool strings;
                locations.load(repository);
                strings.load(repository);
                locations.load(repository);
                strings.load(repository);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedStartupData, RejectedRowsReleaseEveryPreparedAllocation) {
    ASSERT_EXIT(
        {
            StartupDataRepository repository;
            repository.locations.push_back(repository.locations.front());
            repository.strings.front().text = std::string(128, 'a');
            repository.strings.push_back(repository.strings.front());
            ResurrectLocationManager locations;
            StringPool strings;
            AllocationProbe probe;
            unsigned failures = 0;
            try {
                locations.load(repository);
            } catch (const NoSuchElementException&) {
                ++failures;
            }
            try {
                strings.load(repository);
            } catch (const DuplicatedException&) {
                ++failures;
            }
            std::_Exit(failures == 2 && probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

class RefusingBuffer : public std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override {
        return 0;
    }
};

TEST(SharedStartupData, ThrowingLocationDiagnosticsCannotPublishReplacement) {
    ASSERT_EXIT(
        {
            StartupDataRepository repository;
            ResurrectLocationManager locations;
            locations.load(repository);
            repository.locations.front().zoneID = 30;
            repository.locations.back().zoneID = 30;
            RefusingBuffer buffer;
            auto* previousBuffer = std::cerr.rdbuf(&buffer);
            const auto previousExceptions = std::cerr.exceptions();
            std::cerr.exceptions(std::ios::badbit | std::ios::failbit);
            bool failed = false;
            try {
                locations.load(repository);
            } catch (const std::ios_base::failure&) {
                failed = true;
            }
            std::cerr.exceptions(std::ios::goodbit);
            std::cerr.rdbuf(previousBuffer);
            std::cerr.clear();
            std::cerr.exceptions(previousExceptions);
            ZONE_COORD position;
            const bool preserved = locations.getSlayerPosition(10, position) && matches(position, 11, 12, 13) &&
                                   locations.getVampirePosition(10, position) && matches(position, 14, 15, 16) &&
                                   !locations.getSlayerPosition(30, position) &&
                                   !locations.getVampirePosition(30, position);
            std::_Exit(failed && preserved ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

} // namespace
