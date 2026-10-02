#include <array>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Properties.h"
#include "support/AllocationProbe.h"
#include "support/FakeSharedGuildRepository.h"

namespace {

struct GuildIDs {
    GuildID_t guild;
    std::array<ZoneID_t, 3> zones;

    static GuildIDs read() {
        return {Guild::getMaxGuildID(),
                {Guild::getMaxSlayerZoneID(), Guild::getMaxVampireZoneID(), Guild::getMaxOustersZoneID()}};
    }
    void publish() const {
        Guild::setMaxGuildID(guild);
        Guild::setMaxSlayerZoneID(zones[0]);
        Guild::setMaxVampireZoneID(zones[1]);
        Guild::setMaxOustersZoneID(zones[2]);
    }
    bool operator==(const GuildIDs&) const = default;
};

constexpr GuildIDs previousIDs{77, {10000, 20000, 30000}};
constexpr GuildIDs loadedIDs{999, {10001, 22000, 30001}};

// Each test restores the process-wide allocation counters, including on an
// assertion failure. Production initialization runs before the worker starts.
class SharedGuildStartup : public ::testing::Test {
    GuildIDs saved = GuildIDs::read();

    void SetUp() override {
        previousIDs.publish();
    }
    void TearDown() override {
        saved.publish();
    }
};

enum Stage {
    GuildCount,
    GuildMaximum,
    SlayerCount,
    SlayerMaximum,
    VampireCount,
    VampireMaximum,
    OustersCount,
    OustersMaximum,
    GuildRows,
    MemberRows,
    StageCount
};

class StartupRepository : public FakeSharedGuildRepository {
public:
    int countGuilds() override {
        observe(GuildCount);
        return guildCount;
    }
    int loadMaxGuildID() override {
        if (guildCount == 0)
            std::abort(); // The MySQL MAX result would contain NULL.
        observe(GuildMaximum);
        return maximumID;
    }
    int countGuildsOfRace(int race) override {
        checkRace(race);
        observe(static_cast<Stage>(SlayerCount + race * 2));
        return raceCounts[race];
    }
    int loadMaxGuildZoneIDOfRace(int race) override {
        checkRace(race);
        if (raceCounts[race] == 0)
            std::abort();
        observe(static_cast<Stage>(SlayerMaximum + race * 2));
        return maximumZones[race];
    }
    std::vector<SharedGuildListRow> loadGuildsInStates(int stateA, int stateB) override {
        if (stateA != Guild::GUILD_STATE_WAIT || stateB != Guild::GUILD_STATE_ACTIVE)
            std::abort();
        observe(GuildRows);
        return guilds;
    }
    std::vector<SharedGuildMemberListRow> loadActiveMembers() override {
        observe(MemberRows);
        return members;
    }
    void stampMemberRequestDateTime(const std::string&) override {
        std::abort();
    }
    void setCharacterGuildID(GuildRace_t, int, const std::string&) override {
        std::abort();
    }
    void addCharacterGold(GuildRace_t, int, const std::string&) override {
        std::abort();
    }
    void insertMessage(SharedMessageSpelling, const std::string&, const std::string&) override {
        std::abort();
    }

    int guildCount = 3;
    int maximumID = 999;
    std::array<int, 3> raceCounts{1, 1, 1};
    std::array<int, 3> maximumZones{9500, 22000, 30001};
    std::array<unsigned, StageCount> calls{};
    Stage failAt = StageCount;
    std::exception_ptr failure;
    std::optional<GuildIDs> expectedDuringReads;
    bool sawPartialPublication = false;
    std::vector<SharedGuildListRow> guilds = {
        {100, "First guild", Guild::GUILD_TYPE_NORMAL, Guild::GUILD_RACE_SLAYER, Guild::GUILD_STATE_ACTIVE, 1, 10001,
         "Leader", "2026-10-02", "First intro"},
        {200, "Waiting guild", Guild::GUILD_TYPE_NORMAL, Guild::GUILD_RACE_VAMPIRE, Guild::GUILD_STATE_WAIT, 2, 22000,
         "Waiting", "2026-10-02", "Waiting intro"}};
    std::vector<SharedGuildMemberListRow> members = {
        {100, "Leader", GuildMember::GUILDMEMBER_RANK_MASTER, "", 1},
        {200, "Waiting", GuildMember::GUILDMEMBER_RANK_WAIT, "2026-10-01 12:34:56", 0}};

private:
    static void checkRace(int race) {
        if (race < Guild::GUILD_RACE_SLAYER || race >= Guild::GUILD_RACE_MAX)
            std::abort();
    }
    void observe(Stage stage) {
        ++calls[stage];
        if (expectedDuringReads && GuildIDs::read() != *expectedDuringReads)
            sawPartialPublication = true;
        if (stage == failAt)
            std::rethrow_exception(failure);
    }
};

Properties emptyTableConfig(const std::string& dimension = "1", const std::string& world = "2") {
    Properties config;
    config.setProperty("Dimension", dimension);
    config.setProperty("WorldID", world);
    return config;
}

void expectOriginal(const GuildManager& manager, const Guild* guild, const GuildMember* member) {
    EXPECT_EQ(GuildIDs::read(), previousIDs);
    EXPECT_EQ(manager.getGuildSize(), 2u);
    ASSERT_EQ(manager.getGuilds_const().at(100), guild);
    EXPECT_EQ(guild->getMember("Leader"), member);
    EXPECT_EQ(guild->getName(), "First guild");
}

TEST_F(SharedGuildStartup, NonemptyTablesUseStoredMaximaAndPreviousZoneMinimumsWithoutConfigKeys) {
    StartupRepository repository;
    Properties config;
    GuildManager guilds;
    guilds.init(repository, config);
    EXPECT_EQ(GuildIDs::read(), loadedIDs);
    EXPECT_EQ(guilds.getGuildSize(), 2u);
    ASSERT_NE(guilds.getGuild(100), nullptr);
    EXPECT_EQ(guilds.getGuild(100)->getMember("Leader")->getRank(), GuildMember::GUILDMEMBER_RANK_MASTER);
    for (unsigned count : repository.calls)
        EXPECT_EQ(count, 1u);
}

TEST_F(SharedGuildStartup, EmptyTablesUseConfiguredNumberingAndNeverQueryNullMaxima) {
    StartupRepository repository;
    repository.guildCount = 0;
    repository.raceCounts = {0, 0, 0};
    repository.guilds.clear();
    repository.members.clear();
    const auto config = emptyTableConfig();
    GuildManager guilds;
    guilds.init(repository, config);
    const GuildIDs expected{16100, {10001, 20001, 30001}};
    EXPECT_EQ(GuildIDs::read(), expected);
    EXPECT_EQ(guilds.getGuildSize(), 0u);
    for (const auto stage : {GuildMaximum, SlayerMaximum, VampireMaximum, OustersMaximum})
        EXPECT_EQ(repository.calls[stage], 0u);
    EXPECT_EQ(repository.calls[GuildRows], 1u);
    EXPECT_EQ(repository.calls[MemberRows], 1u);
}

TEST_F(SharedGuildStartup, EmptyRacesSkipTheirMaximumEvenWhenOtherRacesHaveGuilds) {
    StartupRepository repository;
    repository.raceCounts = {0, 1, 0};
    Properties config;
    GuildManager guilds;
    guilds.init(repository, config);
    EXPECT_EQ(GuildIDs::read(), loadedIDs);
    EXPECT_EQ(repository.calls[SlayerMaximum], 0u);
    EXPECT_EQ(repository.calls[VampireMaximum], 1u);
    EXPECT_EQ(repository.calls[OustersMaximum], 0u);
}

TEST_F(SharedGuildStartup, NoCounterIsPublishedWhileAnyRepositoryReadCanStillFail) {
    StartupRepository repository;
    repository.expectedDuringReads = previousIDs;
    Properties config;
    GuildManager guilds;
    guilds.init(repository, config);
    EXPECT_FALSE(repository.sawPartialPublication);
    EXPECT_EQ(GuildIDs::read(), loadedIDs);
}

TEST_F(SharedGuildStartup, EveryRepositoryFailurePreservesExceptionIdentityCountersAndBorrowedRowsThenRetries) {
    const std::array failures{std::make_exception_ptr(std::runtime_error("read failed")),
                              std::make_exception_ptr(DatabaseError("database failed")),
                              std::make_exception_ptr(Error("load failed"))};
    for (int stage = GuildCount; stage < StageCount; ++stage) {
        SCOPED_TRACE(stage);
        for (const auto& failure : failures) {
            previousIDs.publish();
            StartupRepository original;
            GuildManager guilds;
            guilds.load(original);
            const auto* previous = guilds.getGuild(100);
            const auto* member = previous->getMember("Leader");
            StartupRepository repository;
            repository.guilds[0].name = "Replacement";
            repository.failAt = static_cast<Stage>(stage);
            repository.failure = failure;
            Properties config;
            std::exception_ptr caught;
            try {
                guilds.init(repository, config);
            } catch (...) {
                caught = std::current_exception();
            }
            EXPECT_EQ(caught, failure);
            expectOriginal(guilds, previous, member);
            repository.failAt = StageCount;
            guilds.init(repository, config);
            EXPECT_EQ(GuildIDs::read(), loadedIDs);
            EXPECT_EQ(guilds.getGuild(100)->getName(), "Replacement");
        }
    }
}

TEST_F(SharedGuildStartup, InvalidStoredMaximaCannotNarrowOrHideBehindThePreviousMinimum) {
    for (unsigned field = 0; field < 4; ++field) {
        SCOPED_TRACE(field);
        for (const int value : {-1, 65536, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
            SCOPED_TRACE(value);
            previousIDs.publish();
            StartupRepository repository;
            GuildManager guilds;
            guilds.load(repository);
            const auto* previous = guilds.getGuild(100);
            const auto* member = previous->getMember("Leader");
            if (field == 0)
                repository.maximumID = value;
            else
                repository.maximumZones[field - 1] = value;
            Properties config;
            EXPECT_THROW(guilds.init(repository, config), Error);
            expectOriginal(guilds, previous, member);
        }
    }
}

TEST_F(SharedGuildStartup, ExhaustedPreviousZoneCountersCannotWrapForEmptyOrPopulatedRaces) {
    for (unsigned race = 0; race < 3; ++race) {
        SCOPED_TRACE(race);
        for (const int count : {0, 1}) {
            SCOPED_TRACE(count);
            GuildIDs exhausted = previousIDs;
            exhausted.zones[race] = std::numeric_limits<ZoneID_t>::max();
            exhausted.publish();
            StartupRepository repository;
            repository.raceCounts[race] = count;
            GuildManager guilds;
            Properties config;
            EXPECT_THROW(guilds.init(repository, config), Error);
            EXPECT_EQ(GuildIDs::read(), exhausted);
            EXPECT_EQ(guilds.getGuildSize(), 0u);
        }
    }
}

TEST_F(SharedGuildStartup, NegativeCountsAreRefusedBeforeQueryingTheirMaxima) {
    for (unsigned field = 0; field < 4; ++field) {
        SCOPED_TRACE(field);
        previousIDs.publish();
        StartupRepository repository;
        if (field == 0)
            repository.guildCount = -1;
        else
            repository.raceCounts[field - 1] = -1;
        GuildManager guilds;
        Properties config;
        EXPECT_THROW(guilds.init(repository, config), Error);
        EXPECT_EQ(GuildIDs::read(), previousIDs);
        EXPECT_EQ(guilds.getGuildSize(), 0u);
        const auto maximum = field == 0 ? GuildMaximum : static_cast<Stage>(SlayerMaximum + (field - 1) * 2);
        EXPECT_EQ(repository.calls[maximum], 0u);
    }
}

TEST_F(SharedGuildStartup, InvalidConfigurationCannotCreateWrappedOrPartiallyParsedGuildIDs) {
    const std::array<std::array<const char*, 2>, 10> invalid = {{{"-1", "4"},
                                                                 {"1", "-1"},
                                                                 {"7", "0"},
                                                                 {"0", "22"},
                                                                 {"1garbage", "0"},
                                                                 {"", "1"},
                                                                 {"1", "2.5"},
                                                                 {"2147483647", "0"},
                                                                 {"0", "2147483647"},
                                                                 {"0", "999999999999999999999"}}};
    for (const auto& pair : invalid) {
        SCOPED_TRACE(pair[0]);
        SCOPED_TRACE(pair[1]);
        previousIDs.publish();
        StartupRepository repository;
        repository.guildCount = 0;
        repository.raceCounts = {0, 0, 0};
        GuildManager guilds;
        const auto config = emptyTableConfig(pair[0], pair[1]);
        EXPECT_THROW(guilds.init(repository, config), Error);
        EXPECT_EQ(GuildIDs::read(), previousIDs);
        EXPECT_EQ(guilds.getGuildSize(), 0u);
    }
}

TEST_F(SharedGuildStartup, MissingConfigurationKeysPreserveThePreviousGraphAndAllowRetry) {
    for (const std::string missing : {"Dimension", "WorldID"}) {
        SCOPED_TRACE(missing);
        previousIDs.publish();
        StartupRepository repository;
        GuildManager guilds;
        guilds.load(repository);
        const auto* previous = guilds.getGuild(100);
        const auto* member = previous->getMember("Leader");
        repository.guildCount = 0;
        repository.raceCounts = {0, 0, 0};
        repository.guilds.clear();
        repository.members.clear();
        Properties config;
        config.setProperty(missing == "Dimension" ? "WorldID" : "Dimension", "1");
        EXPECT_THROW(guilds.init(repository, config), NoSuchElementException);
        expectOriginal(guilds, previous, member);
        config.setProperty(missing, "1");
        guilds.init(repository, config);
        const GuildIDs expected{13100, {10001, 20001, 30001}};
        EXPECT_EQ(GuildIDs::read(), expected);
        EXPECT_EQ(guilds.getGuildSize(), 0u);
    }
}

TEST_F(SharedGuildStartup, ZeroAndPaddedDecimalConfigurationKeepTheExistingNumberingFormula) {
    struct Configuration {
        const char* dimension;
        const char* world;
        GuildID_t expected;
    };
    const std::array<Configuration, 4> configurations = {
        {{"0", "0", 100}, {"2", "15", 65100}, {"6", "1", 63100}, {" \t+0001\r\n", " +02\t", 16100}}};
    for (const auto& item : configurations) {
        SCOPED_TRACE(item.dimension);
        SCOPED_TRACE(item.world);
        previousIDs.publish();
        StartupRepository repository;
        repository.guildCount = 0;
        repository.raceCounts = {0, 0, 0};
        repository.guilds.clear();
        repository.members.clear();
        GuildManager guilds;
        guilds.init(repository, emptyTableConfig(item.dimension, item.world));
        const GuildIDs expected{item.expected, {10001, 20001, 30001}};
        EXPECT_EQ(GuildIDs::read(), expected);
    }
}

TEST_F(SharedGuildStartup, DecimalParsingRejectsEmptySignsEmbeddedNulsAndTrailingText) {
    const std::vector<std::string> invalid{" ",   "+",   "++1", "+-1",  "--1",        "-0",
                                           "0x1", "1e1", "1 2", "1\n2", "4294967296", std::string("1\0ignored", 9)};
    for (const auto& value : invalid) {
        SCOPED_TRACE(value);
        for (const bool dimension : {false, true}) {
            StartupRepository repository;
            repository.guildCount = 0;
            GuildManager guilds;
            const auto config = dimension ? emptyTableConfig(value, "0") : emptyTableConfig("0", value);
            EXPECT_THROW(guilds.init(repository, config), Error);
            EXPECT_EQ(GuildIDs::read(), previousIDs);
            EXPECT_EQ(guilds.getGuildSize(), 0u);
        }
    }
}

TEST_F(SharedGuildStartup, RepresentableStoredBoundariesRemainValid) {
    for (const int maximum : {0, 65535}) {
        SCOPED_TRACE(maximum);
        const GuildIDs zero{0, {0, 0, 0}};
        zero.publish();
        StartupRepository repository;
        repository.maximumID = maximum;
        repository.maximumZones.fill(maximum);
        GuildManager guilds;
        Properties config;
        guilds.init(repository, config);
        const auto expectedZone = static_cast<ZoneID_t>(maximum == 0 ? 1 : maximum);
        const GuildIDs expected{static_cast<GuildID_t>(maximum), {expectedZone, expectedZone, expectedZone}};
        EXPECT_EQ(GuildIDs::read(), expected);
        EXPECT_EQ(guilds.getGuildSize(), 2u);
    }
}

TEST_F(SharedGuildStartup, RefusedGuildsAndRostersCannotPublishPreparedCounters) {
    for (unsigned invalid = 0; invalid < 4; ++invalid) {
        SCOPED_TRACE(invalid);
        previousIDs.publish();
        StartupRepository original;
        GuildManager guilds;
        guilds.load(original);
        const auto* previous = guilds.getGuild(100);
        const auto* member = previous->getMember("Leader");
        StartupRepository repository;
        if (invalid == 0)
            repository.guilds.push_back(repository.guilds.front());
        else if (invalid == 1)
            repository.members.push_back(repository.members.front());
        else if (invalid == 2)
            repository.guilds[1].race = Guild::GUILD_RACE_MAX;
        else
            repository.members[1].rank = GuildMember::GUILDMEMBER_RANK_WAIT + 1;
        Properties config;
        if (invalid < 2)
            EXPECT_THROW(guilds.init(repository, config), DuplicatedException);
        else
            EXPECT_THROW(guilds.init(repository, config), Error);
        expectOriginal(guilds, previous, member);
        guilds.init(original, config);
        EXPECT_EQ(GuildIDs::read(), loadedIDs);
    }
}

int checkStartupAllocationFailure(std::size_t failAt, bool populated, bool emptyTable) {
    previousIDs.publish();
    StartupRepository original;
    StartupRepository repository;
    repository.expectedDuringReads = previousIDs;
    repository.guilds[0].name = "Replacement";
    repository.guilds[0].intro = std::string(192, 'a');
    repository.guilds[1].intro = std::string(160, 'b');
    if (emptyTable) {
        repository.guildCount = 0;
        repository.raceCounts = {0, 0, 0};
        repository.guilds.clear();
        repository.members.clear();
    }
    // Force allocating property copies in the empty-table numbering path.
    const auto config = emptyTableConfig(std::string(96, '0') + "1", std::string(96, '0') + "2");
    auto guilds = std::make_unique<GuildManager>();
    if (populated)
        guilds->load(original);
    const auto* previous = guilds->getGuild(100);
    const auto* member = populated ? previous->getMember("Leader") : nullptr;
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        guilds->init(repository, config);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 128 && failed))
        return 1;
    if (failed) {
        if (GuildIDs::read() != previousIDs || guilds->getGuild(100) != previous ||
            guilds->getGuildSize() != (populated ? 2 : 0))
            return 2;
        if (populated && (previous->getMember("Leader") != member || previous->getName() != "First guild"))
            return 3;
        if (probe.outstanding() != 0)
            return 4;
        guilds->init(repository, config);
    }
    const GuildIDs expected = emptyTable ? GuildIDs{16100, {10001, 20001, 30001}} : loadedIDs;
    if (repository.sawPartialPublication || GuildIDs::read() != expected ||
        guilds->getGuildSize() != (emptyTable ? 0 : 2))
        return 5;
    if (!emptyTable && (!guilds->getGuild(100) || guilds->getGuild(100)->getName() != "Replacement" ||
                        guilds->getGuild(100)->getMember("Leader") == nullptr ||
                        guilds->getGuild(100)->getIntro() != repository.guilds[0].intro))
        return 6;
    guilds.reset();
    return probe.outstanding() == 0 ? 0 : 7;
}

TEST_F(SharedGuildStartup, AllocationFailuresPreserveCountersAndGraphReleasePreparationAndPermitRetry) {
    for (const bool populated : {false, true}) {
        SCOPED_TRACE(populated);
        for (const bool emptyTable : {false, true}) {
            SCOPED_TRACE(emptyTable);
            for (std::size_t failAt = 1; failAt <= 128; ++failAt) {
                SCOPED_TRACE(failAt);
                ASSERT_EXIT(std::_Exit(checkStartupAllocationFailure(failAt, populated, emptyTable)),
                            ::testing::ExitedWithCode(0), "");
            }
        }
    }
}

TEST_F(SharedGuildStartup, ReinitializationAdvancesEachZoneOnlyAfterSuccessAndPlainLoadsLeaveCountersAlone) {
    StartupRepository repository;
    Properties config;
    GuildManager guilds;
    guilds.init(repository, config);
    GuildIDs expected = loadedIDs;
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        SCOPED_TRACE(cycle);
        const auto* previous = guilds.getGuild(100);
        const auto* member = previous->getMember("Leader");
        repository.failAt = MemberRows;
        repository.failure = std::make_exception_ptr(std::runtime_error("roster failed"));
        EXPECT_THROW(guilds.init(repository, config), std::runtime_error);
        EXPECT_EQ(GuildIDs::read(), expected);
        ASSERT_EQ(guilds.getGuild(100), previous);
        EXPECT_EQ(previous->getMember("Leader"), member);
        repository.failAt = StageCount;
        guilds.init(repository, config);
        for (auto& zone : expected.zones)
            ++zone;
        EXPECT_EQ(GuildIDs::read(), expected);
        guilds.load(repository);
        EXPECT_EQ(GuildIDs::read(), expected);
        EXPECT_EQ(guilds.getGuildSize(), 2u);
    }
}

} // namespace
