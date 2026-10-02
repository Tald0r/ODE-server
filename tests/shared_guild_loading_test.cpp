#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "Guild.h"
#include "GuildInfo2.h"
#include "GuildManager.h"
#include "GuildMemberInfo2.h"
#include "SGGuildInfo.h"
#include "support/AllocationProbe.h"
#include "support/FakeSharedGuildRepository.h"

namespace {

// Loading must use only the two supplied tables. Writes and startup ID probes
// abort so an accidental call cannot reach the default database repository.
class GuildRepository : public FakeSharedGuildRepository {
public:
    std::vector<SharedGuildListRow> loadGuildsInStates(int stateA, int stateB) override {
        firstState = stateA;
        secondState = stateB;
        if (guildFailure)
            std::rethrow_exception(guildFailure);
        return guilds;
    }
    std::vector<SharedGuildMemberListRow> loadActiveMembers() override {
        if (memberFailure)
            std::rethrow_exception(memberFailure);
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

    int firstState = -1;
    int secondState = -1;
    std::exception_ptr guildFailure;
    std::exception_ptr memberFailure;
    std::vector<SharedGuildListRow> guilds = {
        {100, "First guild", Guild::GUILD_TYPE_NORMAL, Guild::GUILD_RACE_SLAYER, Guild::GUILD_STATE_ACTIVE, 1, 1001,
         "Leader", "2026-10-02", "First intro"},
        {200, "Second guild", Guild::GUILD_TYPE_ASSASSIN, Guild::GUILD_RACE_VAMPIRE, Guild::GUILD_STATE_WAIT, 2, 2001,
         "Second", "2026-10-01", "Second intro"},
        {300, "Cancelled", Guild::GUILD_TYPE_NORMAL, Guild::GUILD_RACE_OUSTERS, Guild::GUILD_STATE_CANCEL, 3, 3001,
         "Former", "2026-09-30", "Old intro"}};
    std::vector<SharedGuildMemberListRow> members = {
        {100, "Leader", GuildMember::GUILDMEMBER_RANK_MASTER, "", 1},
        {100, "Member", GuildMember::GUILDMEMBER_RANK_NORMAL, "", 0},
        {200, "Waiting", GuildMember::GUILDMEMBER_RANK_WAIT, "2026-10-01 12:34:56", 0}};
};

TEST(SharedGuildLoading, LoadsOnlyWaitingAndActiveGuildsWithTheirFields) {
    GuildRepository repository;
    GuildManager guilds;
    guilds.load(repository);
    EXPECT_EQ(repository.firstState, Guild::GUILD_STATE_WAIT);
    EXPECT_EQ(repository.secondState, Guild::GUILD_STATE_ACTIVE);
    EXPECT_EQ(guilds.getGuildSize(), 2u);
    const auto* first = guilds.getGuild(100);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->getID(), 100);
    EXPECT_EQ(first->getName(), "First guild");
    EXPECT_EQ(first->getType(), Guild::GUILD_TYPE_NORMAL);
    EXPECT_EQ(first->getRace(), Guild::GUILD_RACE_SLAYER);
    EXPECT_EQ(first->getState(), Guild::GUILD_STATE_ACTIVE);
    EXPECT_EQ(first->getServerGroupID(), 1);
    EXPECT_EQ(first->getZoneID(), 1001);
    EXPECT_EQ(first->getMaster(), "Leader");
    EXPECT_EQ(first->getDate(), "2026-10-02");
    EXPECT_EQ(first->getIntro(), "First intro");
    ASSERT_NE(guilds.getGuild(200), nullptr);
    EXPECT_EQ(guilds.getGuild(200)->getState(), Guild::GUILD_STATE_WAIT);
    EXPECT_EQ(guilds.getGuild(300), nullptr);
}

TEST(SharedGuildLoading, LoadsRostersAndCountsWithoutStartupOrDatabaseWrites) {
    GuildRepository repository;
    GuildManager guilds;
    guilds.load(repository);
    const auto* first = guilds.getGuild(100);
    ASSERT_NE(first, nullptr);
    const auto* leader = first->getMember("Leader");
    ASSERT_NE(leader, nullptr);
    EXPECT_EQ(leader->getGuildID(), 100);
    EXPECT_EQ(leader->getName(), "Leader");
    EXPECT_EQ(leader->getRank(), GuildMember::GUILDMEMBER_RANK_MASTER);
    EXPECT_TRUE(leader->getLogOn());
    EXPECT_EQ(first->getActiveMemberCount(), 2);
    EXPECT_EQ(first->getWaitMemberCount(), 0);
    ASSERT_NE(first->getMember("Member"), nullptr);
    EXPECT_FALSE(first->getMember("Member")->getLogOn());
    const auto* second = guilds.getGuild(200);
    ASSERT_NE(second, nullptr);
    const auto* waiting = second->getMember("Waiting");
    ASSERT_NE(waiting, nullptr);
    EXPECT_EQ(waiting->getRequestDateTime(), "2026-10-01 12:34:56");
    EXPECT_EQ(waiting->getRank(), GuildMember::GUILDMEMBER_RANK_WAIT);
    EXPECT_EQ(second->getActiveMemberCount(), 0);
    EXPECT_EQ(second->getWaitMemberCount(), 1);
}

TEST(SharedGuildLoading, ReloadReplacesGuildsAndRostersAndDropsRemovedRows) {
    GuildRepository repository;
    GuildManager guilds;
    guilds.load(repository);
    repository.guilds.resize(1);
    repository.guilds[0].name = "Replacement";
    repository.members = {{100, "New member", GuildMember::GUILDMEMBER_RANK_NORMAL, "", 0}};
    ASSERT_NO_THROW(guilds.load(repository));
    EXPECT_EQ(guilds.getGuildSize(), 1u);
    EXPECT_EQ(guilds.getGuild(200), nullptr);
    const auto* replacement = guilds.getGuild(100);
    ASSERT_NE(replacement, nullptr);
    EXPECT_EQ(replacement->getName(), "Replacement");
    EXPECT_EQ(replacement->getMember("Leader"), nullptr);
    EXPECT_NE(replacement->getMember("New member"), nullptr);
    EXPECT_EQ(replacement->getActiveMemberCount(), 1);
}

TEST(SharedGuildLoading, FailedRosterFetchDoesNotPublishPreparedGuilds) {
    GuildRepository original;
    GuildManager guilds;
    guilds.load(original);
    const auto* previous = guilds.getGuild(100);
    GuildRepository rejected;
    rejected.guilds.resize(1);
    rejected.guilds[0].id = 400;
    rejected.memberFailure = std::make_exception_ptr(std::runtime_error("roster failed"));
    EXPECT_THROW(guilds.load(rejected), std::runtime_error);
    EXPECT_EQ(guilds.getGuildSize(), 2u);
    EXPECT_EQ(guilds.getGuild(100), previous);
    EXPECT_EQ(guilds.getGuild(400), nullptr);
}

TEST(SharedGuildLoading, DuplicateGuildRowsDoNotPublishAPrefix) {
    GuildRepository repository;
    repository.guilds[1].id = 100;
    GuildManager guilds;
    EXPECT_THROW(guilds.load(repository), DuplicatedException);
    EXPECT_EQ(guilds.getGuildSize(), 0u);
    EXPECT_EQ(guilds.getGuild(100), nullptr);
}

TEST(SharedGuildLoading, DuplicateMemberRowsDoNotPublishPartialRosters) {
    GuildRepository repository;
    repository.members.push_back(repository.members.front());
    GuildManager guilds;
    EXPECT_THROW(guilds.load(repository), DuplicatedException);
    EXPECT_EQ(guilds.getGuildSize(), 0u);
    EXPECT_EQ(guilds.getGuild(100), nullptr);
}

TEST(SharedGuildLoading, MembersWithoutALoadedGuildAreReleased) {
    ASSERT_EXIT(
        {
            GuildRepository repository;
            repository.guilds.clear();
            AllocationProbe probe;
            {
                GuildManager guilds;
                guilds.load(repository);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

int checkStringAllocationFailure(unsigned field, bool getter) {
    const std::string value(96, 'a');
    Guild guild;
    GuildMember member;
    if (getter) {
        guild.setName(value);
        guild.setMaster(value);
        guild.setDate(value);
        guild.setIntro(value);
        member.setName(value);
    }
    AllocationProbe probe(1);
    bool failed = false;
    try {
        if (getter) {
            switch (field) {
            case 0:
                guild.getName();
                break;
            case 1:
                guild.getMaster();
                break;
            case 2:
                guild.getDate();
                break;
            case 3:
                guild.getIntro();
                break;
            case 4:
                member.getName();
                break;
            }
        } else {
            switch (field) {
            case 0:
                guild.setName(value);
                break;
            case 1:
                guild.setMaster(value);
                break;
            case 2:
                guild.setDate(value);
                break;
            case 3:
                guild.setIntro(value);
                break;
            case 4:
                member.setName(value);
                break;
            }
        }
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    return failed && probe.rejected() && probe.outstanding() == 0 ? 0 : 1;
}

TEST(SharedGuildLoading, StringSetterAllocationFailuresCanUnwind) {
    for (unsigned field = 0; field < 5; ++field) {
        SCOPED_TRACE(field);
        ASSERT_EXIT(std::_Exit(checkStringAllocationFailure(field, false)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(SharedGuildLoading, StringGetterAllocationFailuresCanUnwind) {
    for (unsigned field = 0; field < 5; ++field) {
        SCOPED_TRACE(field);
        ASSERT_EXIT(std::_Exit(checkStringAllocationFailure(field, true)), ::testing::ExitedWithCode(0), "");
    }
}

GuildRepository replacementRepository() {
    GuildRepository replacement;
    replacement.guilds.resize(2);
    replacement.guilds[0].name = "Replacement";
    replacement.guilds[0].intro = std::string(192, 'a');
    replacement.guilds[1].id = 400;
    replacement.guilds[1].intro = std::string(160, 'b');
    replacement.members = {{100, "New leader", GuildMember::GUILDMEMBER_RANK_MASTER, "", 1},
                           {100, "New member", GuildMember::GUILDMEMBER_RANK_NORMAL, "", 0},
                           {400, "Waiting", GuildMember::GUILDMEMBER_RANK_WAIT, "2026-10-01 12:34:56", 0},
                           {999, "Unattached", GuildMember::GUILDMEMBER_RANK_NORMAL, "", 0}};
    return replacement;
}

int checkLoadAllocationFailure(std::size_t failAt, bool populated) {
    GuildRepository original;
    GuildRepository replacement = replacementRepository();
    auto guilds = std::make_unique<GuildManager>();
    if (populated)
        guilds->load(original);
    const auto* previous = guilds->getGuild(100);
    const auto* previousMember = populated ? previous->getMember("Leader") : nullptr;
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        guilds->load(replacement);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 128 && failed))
        return 1;
    if (failed) {
        if (guilds->getGuild(100) != previous || guilds->getGuildSize() != (populated ? 2 : 0) ||
            guilds->getGuild(400) != nullptr)
            return 2;
        if (populated && (previous->getMember("Leader") != previousMember || previous->getName() != "First guild" ||
                          previous->getActiveMemberCount() != 2))
            return 3;
        if (probe.outstanding() != 0)
            return 4;
        guilds->load(replacement);
    }
    const auto* first = guilds->getGuild(100);
    const auto* second = guilds->getGuild(400);
    if (!first || !second || guilds->getGuildSize() != 2 || guilds->getGuild(200) != nullptr ||
        first->getName() != "Replacement" || first->getIntro() != replacement.guilds[0].intro ||
        first->getMember("Leader") != nullptr || first->getMember("New leader") == nullptr ||
        first->getActiveMemberCount() != 2 || second->getMember("Waiting") == nullptr ||
        second->getActiveMemberCount() != 0 || second->getWaitMemberCount() != 1)
        return 5;
    guilds.reset();
    return probe.outstanding() == 0 ? 0 : 6;
}

TEST(SharedGuildLoading, AllocationFailuresPreserveEmptyAndPopulatedGraphsAndPermitRetry) {
    for (const bool populated : {false, true}) {
        SCOPED_TRACE(populated);
        for (std::size_t failAt = 1; failAt <= 128; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(std::_Exit(checkLoadAllocationFailure(failAt, populated)), ::testing::ExitedWithCode(0), "");
        }
    }
}

int checkDuplicateCleanup(bool duplicateMember) {
    GuildRepository original;
    GuildRepository replacement = replacementRepository();
    GuildRepository rejected = replacement;
    if (duplicateMember)
        rejected.members.push_back(rejected.members.front());
    else
        rejected.guilds.push_back(rejected.guilds.front());
    auto guilds = std::make_unique<GuildManager>();
    guilds->load(original);
    const auto* previous = guilds->getGuild(100);
    const auto* previousMember = previous->getMember("Leader");
    AllocationProbe probe;
    bool refused = false;
    try {
        guilds->load(rejected);
    } catch (const DuplicatedException&) {
        refused = true;
    }
    if (!refused || guilds->getGuild(100) != previous || previous->getMember("Leader") != previousMember ||
        guilds->getGuildSize() != 2 || probe.outstanding() != 0)
        return 1;
    guilds->load(replacement);
    guilds.reset();
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(SharedGuildLoading, DuplicateRefusalReleasesPreparedGraphsAndPreservesBorrowedRows) {
    for (const bool duplicateMember : {false, true}) {
        SCOPED_TRACE(duplicateMember);
        ASSERT_EXIT(std::_Exit(checkDuplicateCleanup(duplicateMember)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(SharedGuildLoading, InvalidGuildFieldsCannotNarrowOrReplaceThePreviousGraph) {
    GuildRepository original;
    GuildManager guilds;
    guilds.load(original);
    const auto* previous = guilds.getGuild(100);
    using Field = int SharedGuildListRow::*;
    const std::vector<std::pair<Field, int>> invalid = {{&SharedGuildListRow::id, -1},
                                                        {&SharedGuildListRow::id, 65536},
                                                        {&SharedGuildListRow::id, std::numeric_limits<int>::max()},
                                                        {&SharedGuildListRow::serverGroupID, -1},
                                                        {&SharedGuildListRow::serverGroupID, 256},
                                                        {&SharedGuildListRow::zoneID, -1},
                                                        {&SharedGuildListRow::zoneID, 65536},
                                                        {&SharedGuildListRow::type, -1},
                                                        {&SharedGuildListRow::type, Guild::GUILD_TYPE_MAX},
                                                        {&SharedGuildListRow::race, -1},
                                                        {&SharedGuildListRow::race, Guild::GUILD_RACE_MAX},
                                                        {&SharedGuildListRow::state, -1},
                                                        {&SharedGuildListRow::state, Guild::GUILD_STATE_MAX},
                                                        {&SharedGuildListRow::state, 256}};
    unsigned index = 0;
    for (const auto& [field, value] : invalid) {
        SCOPED_TRACE(index++);
        GuildRepository rejected = replacementRepository();
        rejected.guilds[1].*field = value;
        EXPECT_THROW(guilds.load(rejected), Error);
        EXPECT_EQ(guilds.getGuildSize(), 2u);
        EXPECT_EQ(guilds.getGuild(100), previous);
        EXPECT_EQ(previous->getName(), "First guild");
        EXPECT_NE(previous->getMember("Leader"), nullptr);
        EXPECT_EQ(guilds.getGuild(400), nullptr);
    }
}

TEST(SharedGuildLoading, InvalidMemberIDsAndRanksCannotReplaceThePreviousGraph) {
    GuildRepository original;
    GuildManager guilds;
    guilds.load(original);
    const auto* previous = guilds.getGuild(100);
    const auto* previousMember = previous->getMember("Leader");
    using Field = int SharedGuildMemberListRow::*;
    const std::vector<std::pair<Field, int>> invalid = {
        {&SharedGuildMemberListRow::guildID, -1},
        {&SharedGuildMemberListRow::guildID, 65536},
        {&SharedGuildMemberListRow::guildID, std::numeric_limits<int>::max()},
        {&SharedGuildMemberListRow::rank, -1},
        {&SharedGuildMemberListRow::rank, GuildMember::GUILDMEMBER_RANK_DENY},
        {&SharedGuildMemberListRow::rank, 256}};
    unsigned index = 0;
    for (const auto& [field, value] : invalid) {
        SCOPED_TRACE(index++);
        GuildRepository rejected = replacementRepository();
        rejected.members[1].*field = value;
        EXPECT_THROW(guilds.load(rejected), Error);
        EXPECT_EQ(guilds.getGuildSize(), 2u);
        EXPECT_EQ(guilds.getGuild(100), previous);
        EXPECT_EQ(previous->getMember("Leader"), previousMember);
        EXPECT_EQ(previous->getActiveMemberCount(), 2);
        EXPECT_EQ(guilds.getGuild(400), nullptr);
    }
}

TEST(SharedGuildLoading, FilteredGuildsAndTheirMembersStayOutsideTheReplacement) {
    GuildRepository repository;
    repository.guilds[2].type = -1;
    repository.guilds[2].race = -1;
    repository.guilds[2].serverGroupID = 256;
    repository.guilds[2].zoneID = 65536;
    auto broken = repository.guilds[2];
    broken.id = 400;
    broken.state = Guild::GUILD_STATE_BROKEN;
    repository.guilds.push_back(broken);
    repository.members.push_back({300, "Cancelled", -1, "not a date", 0});
    repository.members.push_back({400, "Broken", 256, "not a date", 0});
    GuildManager guilds;
    EXPECT_NO_THROW(guilds.load(repository));
    EXPECT_EQ(guilds.getGuildSize(), 2u);
    EXPECT_EQ(guilds.getGuild(300), nullptr);
    EXPECT_EQ(guilds.getGuild(400), nullptr);
}

TEST(SharedGuildLoading, ZeroAndMaximumIDsAndNamesInDifferentGuildsRemainIndependent) {
    GuildRepository repository;
    repository.guilds.resize(2);
    repository.guilds[0].id = 0;
    repository.guilds[0].serverGroupID = 0;
    repository.guilds[0].zoneID = 0;
    repository.guilds[1].id = 65535;
    repository.guilds[1].serverGroupID = 255;
    repository.guilds[1].zoneID = 65535;
    repository.guilds[1].race = Guild::GUILD_RACE_OUSTERS;
    repository.members = {{0, "Same name", GuildMember::GUILDMEMBER_RANK_NORMAL, "", 0},
                          {65535, "Same name", GuildMember::GUILDMEMBER_RANK_WAIT, "2026-10-01 12:34:56", 1}};
    GuildManager guilds;
    guilds.load(repository);
    ASSERT_NE(guilds.getGuild(0), nullptr);
    ASSERT_NE(guilds.getGuild(65535), nullptr);
    EXPECT_EQ(guilds.getGuild(0)->getZoneID(), 0);
    EXPECT_EQ(guilds.getGuild(65535)->getServerGroupID(), 255);
    EXPECT_EQ(guilds.getGuild(65535)->getZoneID(), 65535);
    EXPECT_EQ(guilds.getGuild(65535)->getRace(), Guild::GUILD_RACE_OUSTERS);
    const auto* first = guilds.getGuild(0)->getMember("Same name");
    const auto* second = guilds.getGuild(65535)->getMember("Same name");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_EQ(first->getGuildID(), 0);
    EXPECT_EQ(second->getGuildID(), 65535);
    EXPECT_EQ(first->getRank(), GuildMember::GUILDMEMBER_RANK_NORMAL);
    EXPECT_EQ(second->getRank(), GuildMember::GUILDMEMBER_RANK_WAIT);
}

TEST(SharedGuildLoading, WaitingDateFallbackAndNonzeroLogOnKeepTheirExistingInterpretation) {
    GuildRepository repository;
    repository.members[2].requestDateTime = "not a date";
    repository.members[0].logOn = -7;
    repository.members.push_back({100, "Deputy", GuildMember::GUILDMEMBER_RANK_SUBMASTER, "ignored", 0});
    GuildManager guilds;
    guilds.load(repository);
    EXPECT_EQ(guilds.getGuild(200)->getMember("Waiting")->getRequestDateTime(), "2000-01-01 00:00:00");
    EXPECT_TRUE(guilds.getGuild(100)->getMember("Leader")->getLogOn());
    EXPECT_EQ(guilds.getGuild(100)->getActiveMemberCount(), 3);
    EXPECT_EQ(guilds.getGuild(200)->getWaitMemberCount(), 1);
}

TEST(SharedGuildLoading, RepositoryFailuresPreserveExceptionIdentityAndBorrowedRowsAtBothStages) {
    GuildRepository original;
    GuildManager guilds;
    guilds.load(original);
    const auto* previous = guilds.getGuild(100);
    const auto* previousMember = previous->getMember("Leader");
    for (const bool failRoster : {false, true}) {
        SCOPED_TRACE(failRoster);
        for (const auto& failure : {std::make_exception_ptr(std::runtime_error("standard failure")),
                                    std::make_exception_ptr(DatabaseError("database failure")),
                                    std::make_exception_ptr(Error("legacy failure"))}) {
            GuildRepository rejected = replacementRepository();
            (failRoster ? rejected.memberFailure : rejected.guildFailure) = failure;
            try {
                guilds.load(rejected);
                FAIL() << "expected the original repository failure";
            } catch (...) {
                EXPECT_EQ(std::current_exception(), failure);
            }
            EXPECT_EQ(guilds.getGuildSize(), 2u);
            EXPECT_EQ(guilds.getGuild(100), previous);
            EXPECT_EQ(previous->getMember("Leader"), previousMember);
            EXPECT_EQ(guilds.getGuild(400), nullptr);
        }
    }
    GuildRepository replacement = replacementRepository();
    guilds.load(replacement);
    EXPECT_EQ(guilds.getGuild(100)->getName(), "Replacement");
}

TEST(SharedGuildLoading, EmptyGuildTablesReplacePreviousRowsAndPermitReload) {
    GuildRepository original;
    GuildRepository empty;
    empty.guilds.clear(); // Its unattached roster must not resurrect old guilds.
    GuildManager guilds;
    guilds.load(original);
    guilds.load(empty);
    EXPECT_EQ(guilds.getGuildSize(), 0u);
    EXPECT_EQ(guilds.getGuild(100), nullptr);
    guilds.load(original);
    EXPECT_EQ(guilds.getGuildSize(), 2u);
    EXPECT_NE(guilds.getGuild(100)->getMember("Leader"), nullptr);
}

TEST(SharedGuildLoading, RepeatedLoadsAndDestructionReleaseAllGuildAndMemberAllocations) {
    ASSERT_EXIT(
        {
            GuildRepository original;
            GuildRepository replacement = replacementRepository();
            GuildRepository empty;
            empty.guilds.clear();
            AllocationProbe probe;
            {
                GuildManager guilds;
                guilds.load(original);
                guilds.load(replacement);
                guilds.load(replacement);
                guilds.load(empty);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedGuildLoading, LoadedGuildsAndRostersReachTheProductionGuildInfoReply) {
    GuildRepository repository;
    GuildManager guilds;
    guilds.load(repository);
    SGGuildInfo reply;
    guilds.makeSGGuildInfo(reply);
    unsigned guildCount = 0;
    unsigned memberCount = 0;
    while (auto* row = reply.popFrontGuildInfoList()) {
        std::unique_ptr<GuildInfo2> info(row);
        ++guildCount;
        const auto* source = guilds.getGuild(info->getID());
        ASSERT_NE(source, nullptr);
        EXPECT_EQ(info->getName(), source->getName());
        EXPECT_EQ(info->getIntro(), source->getIntro());
        while (auto* memberRow = info->popFrontGuildMemberInfoList()) {
            std::unique_ptr<GuildMemberInfo2> member(memberRow);
            ++memberCount;
            const auto* sourceMember = source->getMember(member->getName());
            ASSERT_NE(sourceMember, nullptr);
            EXPECT_EQ(member->getGuildID(), sourceMember->getGuildID());
            EXPECT_EQ(member->getRank(), sourceMember->getRank());
            EXPECT_EQ(member->getLogOn(), sourceMember->getLogOn());
        }
    }
    EXPECT_EQ(guildCount, 2u);
    EXPECT_EQ(memberCount, 3u);
}

} // namespace
