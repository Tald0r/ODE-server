#include <memory>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

#include "CLLogin.h"
#include "GCAddStoreItem.h"
#include "GSRequestGuildInfo.h"
#include "GameServerPlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "PacketDispatcher.h"
#include "SGGuildInfo.h"
#include "SharedContext.h"
#include "SharedPacketDispatch.h"

#if !defined(__SHARED_SERVER__) || defined(__GAME_SERVER__) || defined(__LOGIN_SERVER__)
#error "SharedServerRuntime must supply only the sharedserver's compile definitions"
#endif

namespace {

// Only the outgoing transport is replaced. Registration, dispatch, the
// handler, guild lookup and packet construction are the production objects.
class RecordingGameServerPlayer : public GameServerPlayer {
public:
    void sendPacket(Packet* packet) override {
        ++replies;
        auto* info = dynamic_cast<SGGuildInfo*>(packet);
        ASSERT_NE(nullptr, info);
        while (auto* guild = info->popFrontGuildInfoList())
            guilds.emplace_back(guild);
    }

    unsigned replies = 0;
    std::vector<std::unique_ptr<GuildInfo2>> guilds;
};

class SharedServerRuntimeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        static std::once_flag registration;
        std::call_once(registration, registerSharedServerPacketHandlers);
    }

    void SetUp() override {
        de::sharedContext().setGuildManager(&guilds);
    }

    void TearDown() override {
        de::sharedContext().setGuildManager(nullptr);
    }

    GuildManager guilds;
};

TEST_F(SharedServerRuntimeTest, ReturnsAnEmptyGuildList) {
    RecordingGameServerPlayer player;
    GSRequestGuildInfo packet;

    ASSERT_NO_THROW(PacketDispatcher::dispatch(&packet, &player));

    EXPECT_EQ(1u, player.replies);
    EXPECT_TRUE(player.guilds.empty());
}

TEST_F(SharedServerRuntimeTest, ReturnsTheLoadedGuildDetails) {
    auto guild = std::make_unique<Guild>();
    guild->setID(1234);
    guild->setName("Test guild");
    guild->setType(1);
    guild->setRace(Guild::GUILD_RACE_SLAYER);
    guild->setState(Guild::GUILD_STATE_ACTIVE);
    guild->setServerGroupID(2);
    guild->setZoneID(1201);
    guild->setMaster("Leader");
    guild->setDate("2026-10-02");
    guild->setIntro("Guild information from memory");
    guilds.addGuild(guild.get());
    guild.release(); // GuildManager owns its entries.

    RecordingGameServerPlayer player;
    GSRequestGuildInfo packet;
    ASSERT_NO_THROW(PacketDispatcher::dispatch(&packet, &player));

    EXPECT_EQ(1u, player.replies);
    ASSERT_EQ(1u, player.guilds.size());
    const GuildInfo2& reply = *player.guilds.front();
    EXPECT_EQ(1234, reply.getID());
    EXPECT_EQ("Test guild", reply.getName());
    EXPECT_EQ(1, reply.getType());
    EXPECT_EQ(Guild::GUILD_RACE_SLAYER, reply.getRace());
    EXPECT_EQ(Guild::GUILD_STATE_ACTIVE, reply.getState());
    EXPECT_EQ(2, reply.getServerGroupID());
    EXPECT_EQ(1201, reply.getZoneID());
    EXPECT_EQ("Leader", reply.getMaster());
    EXPECT_EQ("2026-10-02", reply.getDate());
    EXPECT_EQ("Guild information from memory", reply.getIntro());
    EXPECT_EQ(0, reply.getGuildMemberInfoListNum());
}

TEST_F(SharedServerRuntimeTest, RejectsLoginRequests) {
    CLLogin packet;
    EXPECT_THROW(PacketDispatcher::dispatch(&packet, nullptr), InvalidProtocolException);
}

TEST_F(SharedServerRuntimeTest, RejectsGameserverCompatibilityPackets) {
    GCAddStoreItem packet;
    EXPECT_THROW(PacketDispatcher::dispatch(&packet, nullptr), InvalidProtocolException);
}

} // namespace
