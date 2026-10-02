#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Guild.h"
#include "GuildInfo2.h"
#include "GuildManager.h"
#include "GuildMemberInfo2.h"
#include "SGGuildInfo.h"
#include "support/AllocationProbe.h"

namespace {

std::unique_ptr<Guild> sourceGuild(GuildID_t id) {
    auto guild = std::make_unique<Guild>();
    guild->setID(id);
    guild->setName("Reply guild " + std::to_string(id));
    guild->setType(Guild::GUILD_TYPE_ASSASSIN);
    guild->setRace(Guild::GUILD_RACE_VAMPIRE);
    guild->setState(Guild::GUILD_STATE_WAIT);
    guild->setServerGroupID(4);
    guild->setZoneID(22000 + id);
    guild->setMaster("Leader");
    guild->setDate("2026-10-02");
    guild->setIntro(std::string(180, 'a'));
    for (const auto rank : {GuildMember::GUILDMEMBER_RANK_MASTER, GuildMember::GUILDMEMBER_RANK_NORMAL,
                            GuildMember::GUILDMEMBER_RANK_WAIT}) {
        auto member = std::make_unique<GuildMember>();
        member->setGuildID(id);
        member->setName(rank == GuildMember::GUILDMEMBER_RANK_MASTER ? "Leader" : "Member" + std::to_string(rank));
        member->setRank(rank);
        member->setLogOn(rank != GuildMember::GUILDMEMBER_RANK_WAIT);
        guild->addMember(member.get());
        member.release();
    }
    return guild;
}

void populate(GuildManager& manager) {
    for (const GuildID_t id : {100, 200}) {
        auto guild = sourceGuild(id);
        manager.addGuild(guild.get());
        guild.release();
    }
}

GuildInfo2* addExisting(SGGuildInfo& packet, GuildID_t id = 999) {
    auto info = std::make_unique<GuildInfo2>();
    info->setID(id);
    info->setName("Previous reply");
    info->setType(Guild::GUILD_TYPE_NORMAL);
    info->setRace(Guild::GUILD_RACE_SLAYER);
    info->setState(Guild::GUILD_STATE_ACTIVE);
    info->setServerGroupID(1);
    info->setZoneID(10001);
    info->setMaster("Previous");
    info->setDate("2026-10-01");
    info->setIntro("Existing destination row");
    // The fixture always has space and is built before allocation probes.
    auto* borrowed = info.get();
    packet.addGuildInfo(info.release());
    return borrowed;
}

TEST(SharedGuildReplies, EmptyManagerPreservesExistingDestinationRows) {
    GuildManager manager;
    SGGuildInfo reply;
    auto* previous = addExisting(reply);
    manager.makeSGGuildInfo(reply);
    ASSERT_EQ(reply.getGuildInfoListNum(), 1u);
    std::unique_ptr<GuildInfo2> row(reply.popFrontGuildInfoList());
    EXPECT_EQ(row.get(), previous);
    EXPECT_EQ(row->getName(), "Previous reply");
}

TEST(SharedGuildReplies, PrependsCompleteGuildsAndMembersInTheExistingTraversalOrder) {
    GuildManager manager;
    populate(manager);
    std::vector<Guild*> traversal;
    for (const auto& [id, guild] : manager.getGuilds_const())
        traversal.push_back(guild);
    SGGuildInfo reply;
    auto* previous = addExisting(reply);
    manager.makeSGGuildInfo(reply);
    ASSERT_EQ(reply.getGuildInfoListNum(), 3u);
    for (auto source = traversal.rbegin(); source != traversal.rend(); ++source) {
        std::unique_ptr<GuildInfo2> row(reply.popFrontGuildInfoList());
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(row->getID(), (*source)->getID());
        EXPECT_EQ(row->getName(), (*source)->getName());
        EXPECT_EQ(row->getType(), (*source)->getType());
        EXPECT_EQ(row->getRace(), (*source)->getRace());
        EXPECT_EQ(row->getState(), (*source)->getState());
        EXPECT_EQ(row->getServerGroupID(), (*source)->getServerGroupID());
        EXPECT_EQ(row->getZoneID(), (*source)->getZoneID());
        EXPECT_EQ(row->getMaster(), (*source)->getMaster());
        EXPECT_EQ(row->getDate(), (*source)->getDate());
        EXPECT_EQ(row->getIntro(), (*source)->getIntro());
        std::vector<GuildMember*> members;
        for (const auto& [name, member] : (*source)->getMembers())
            members.push_back(member);
        ASSERT_EQ(row->getGuildMemberInfoListNum(), members.size());
        for (auto sourceMember = members.rbegin(); sourceMember != members.rend(); ++sourceMember) {
            std::unique_ptr<GuildMemberInfo2> member(row->popFrontGuildMemberInfoList());
            ASSERT_NE(member, nullptr);
            EXPECT_EQ(member->getGuildID(), (*sourceMember)->getGuildID());
            EXPECT_EQ(member->getName(), (*sourceMember)->getName());
            EXPECT_EQ(member->getRank(), (*sourceMember)->getRank());
            EXPECT_EQ(member->getLogOn(), (*sourceMember)->getLogOn());
        }
    }
    std::unique_ptr<GuildInfo2> oldRow(reply.popFrontGuildInfoList());
    EXPECT_EQ(oldRow.get(), previous);
    EXPECT_EQ(reply.getGuildInfoListNum(), 0u);
}

int checkReplyAllocation(std::size_t failAt, bool populated, bool requirePreservation) {
    GuildManager manager;
    populate(manager);
    const auto* original = manager.getGuild(100);
    const auto* member = original->getMember("Leader");
    SGGuildInfo reply;
    auto* previous = populated ? addExisting(reply) : nullptr;
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        manager.makeSGGuildInfo(reply);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 128 && failed))
        return 1;
    if (manager.getGuild(100) != original || original->getMember("Leader") != member)
        return 2;
    if (failed && requirePreservation) {
        if (reply.getGuildInfoListNum() != (populated ? 1 : 0))
            return 3;
        if (populated) {
            std::unique_ptr<GuildInfo2> row(reply.popFrontGuildInfoList());
            if (row.get() != previous || row->getName() != "Previous reply")
                return 4;
        }
        if (probe.outstanding() != 0)
            return 5;
        manager.makeSGGuildInfo(reply);
        if (reply.getGuildInfoListNum() != 2)
            return 6;
    }
    reply.clearGuildInfoList();
    return probe.outstanding() == 0 ? 0 : 7;
}

TEST(SharedGuildReplies, AllocationFailuresReleasePartialGuildAndMemberRecords) {
    for (std::size_t failAt = 1; failAt <= 128; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkReplyAllocation(failAt, false, false)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(SharedGuildReplies, AllocationFailuresPreserveEmptyAndPopulatedDestinationsAndAllowRetry) {
    for (const bool populated : {false, true}) {
        SCOPED_TRACE(populated);
        for (std::size_t failAt = 1; failAt <= 128; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(std::_Exit(checkReplyAllocation(failAt, populated, true)), ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(SharedGuildReplies, CountRefusalPreservesExistingRowsAndRetryCanUseFreedCapacity) {
    GuildManager manager;
    populate(manager);
    SGGuildInfo reply;
    std::vector<GuildInfo2*> previous;
    for (unsigned index = 0; index < GuildInfo2::kMaxCount - 1; ++index)
        previous.push_back(addExisting(reply, 1000 + index));
    EXPECT_THROW(manager.makeSGGuildInfo(reply), InvalidProtocolException);
    ASSERT_EQ(reply.getGuildInfoListNum(), previous.size());
    std::unique_ptr<GuildInfo2> removed(reply.popFrontGuildInfoList());
    ASSERT_EQ(removed.get(), previous.back());
    previous.pop_back();
    manager.makeSGGuildInfo(reply);
    EXPECT_EQ(reply.getGuildInfoListNum(), GuildInfo2::kMaxCount);
    for (unsigned index = 0; index < 2; ++index) {
        std::unique_ptr<GuildInfo2> row(reply.popFrontGuildInfoList());
        EXPECT_NE(manager.getGuild(row->getID()), nullptr);
    }
    for (auto old = previous.rbegin(); old != previous.rend(); ++old) {
        std::unique_ptr<GuildInfo2> row(reply.popFrontGuildInfoList());
        EXPECT_EQ(row.get(), *old);
        EXPECT_EQ(row->getName(), "Previous reply");
    }
    EXPECT_EQ(reply.getGuildInfoListNum(), 0u);
}

int checkMemberPreparation(std::size_t failAt) {
    auto guild = sourceGuild(100);
    AllocationProbe probe(failAt);
    bool failed = false;
    {
        GuildInfo2 reply;
        try {
            guild->makeInfo(&reply);
        } catch (const std::bad_alloc&) {
            failed = true;
        }
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed))
        return 1;
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(SharedGuildReplies, FailedIndividualGuildPreparationReleasesUnattachedMembers) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkMemberPreparation(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(SharedGuildReplies, PacketAdderConsumesItsIncomingRecordWhenListAllocationFails) {
    ASSERT_EXIT(
        {
            AllocationProbe probe(2); // record allocation succeeds; list node allocation fails
            bool failed = false;
            {
                SGGuildInfo reply;
                try {
                    reply.addGuildInfo(new GuildInfo2());
                } catch (const std::bad_alloc&) {
                    failed = true;
                }
            }
            probe.stopFailing();
            std::_Exit(failed && probe.rejected() && probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedGuildReplies, CompletedBatchTransferPreservesOrderAndSelfTransferWithoutAllocating) {
    ASSERT_EXIT(
        {
            SGGuildInfo destination;
            auto* previous = addExisting(destination, 999);
            SGGuildInfo source;
            auto* first = addExisting(source, 100);
            auto* second = addExisting(source, 200);
            AllocationProbe probe(1);
            destination.prependGuildInfosFrom(source);
            destination.prependGuildInfosFrom(source); // now empty
            destination.prependGuildInfosFrom(destination);
            if (probe.attempts() != 0 || source.getGuildInfoListNum() != 0 || destination.getGuildInfoListNum() != 3)
                std::_Exit(1);
            std::unique_ptr<GuildInfo2> row1(destination.popFrontGuildInfoList());
            std::unique_ptr<GuildInfo2> row2(destination.popFrontGuildInfoList());
            std::unique_ptr<GuildInfo2> row3(destination.popFrontGuildInfoList());
            std::_Exit(row1.get() == second && row2.get() == first && row3.get() == previous ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedGuildReplies, RefusedBatchTransferRetainsBothOwnersAndCanRetryAfterFreeingSpace) {
    SGGuildInfo destination;
    for (unsigned index = 0; index < GuildInfo2::kMaxCount - 1; ++index)
        addExisting(destination, 1000 + index);
    SGGuildInfo source;
    auto* first = addExisting(source, 100);
    auto* second = addExisting(source, 200);
    EXPECT_THROW(destination.prependGuildInfosFrom(source), InvalidProtocolException);
    ASSERT_EQ(source.getGuildInfoListNum(), 2u);
    ASSERT_EQ(destination.getGuildInfoListNum(), GuildInfo2::kMaxCount - 1);
    std::unique_ptr<GuildInfo2> removed(destination.popFrontGuildInfoList());
    destination.prependGuildInfosFrom(source);
    EXPECT_EQ(source.getGuildInfoListNum(), 0u);
    ASSERT_EQ(destination.getGuildInfoListNum(), GuildInfo2::kMaxCount);
    std::unique_ptr<GuildInfo2> row1(destination.popFrontGuildInfoList());
    std::unique_ptr<GuildInfo2> row2(destination.popFrontGuildInfoList());
    EXPECT_EQ(row1.get(), second);
    EXPECT_EQ(row2.get(), first);
}

TEST(SharedGuildReplies, ReplyIntroTruncationLeavesTheSourceGuildUnchanged) {
    GuildManager manager;
    auto guild = sourceGuild(100);
    const std::string intro(400, 'b');
    guild->setIntro(intro);
    manager.addGuild(guild.get());
    guild.release();
    SGGuildInfo reply;
    manager.makeSGGuildInfo(reply);
    std::unique_ptr<GuildInfo2> row(reply.popFrontGuildInfoList());
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(row->getIntro(), intro.substr(0, GUILD_INTRO_MAX_LENGTH));
    EXPECT_EQ(manager.getGuild(100)->getIntro(), intro);
    EXPECT_EQ(row->getGuildMemberInfoListNum(), 3u);
}

TEST(SharedGuildReplies, RepeatedReplyPreparationAndDestructionReleaseEveryAllocation) {
    ASSERT_EXIT(
        {
            GuildManager manager;
            populate(manager);
            AllocationProbe probe;
            for (unsigned attempt = 0; attempt < 8; ++attempt) {
                SGGuildInfo reply;
                manager.makeSGGuildInfo(reply);
                manager.makeSGGuildInfo(reply);
                if (reply.getGuildInfoListNum() != 4)
                    std::_Exit(1);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

int checkAdderCountRefusal(std::size_t failAt) {
    SGGuildInfo reply;
    for (unsigned index = 0; index < GuildInfo2::kMaxCount; ++index)
        addExisting(reply, 1000 + index);
    AllocationProbe probe(failAt);
    bool refused = false;
    try {
        reply.addGuildInfo(new GuildInfo2());
    } catch (const InvalidProtocolException&) {
        refused = true;
    } catch (const std::bad_alloc&) {
        refused = true;
    }
    probe.stopFailing();
    return refused && reply.getGuildInfoListNum() == GuildInfo2::kMaxCount && probe.outstanding() == 0 ? 0 : 1;
}

TEST(SharedGuildReplies, CountRefusalConsumesTheIncomingRecordEvenWhenItsDiagnosticCannotAllocate) {
    for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkAdderCountRefusal(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

} // namespace
