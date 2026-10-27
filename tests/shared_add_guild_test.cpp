#include <limits>

#include <gtest/gtest.h>

#include "GSAddGuild.h"
#include "Guild.h"

namespace {

constexpr GuildID_t kFullGuildIDs = std::numeric_limits<GuildID_t>::max();
constexpr ZoneID_t kLastZoneID = std::numeric_limits<ZoneID_t>::max();

// Each case founds through the production handler on a refused path, which
// returns before any database write; the fixture puts Guild's counters back.
class SharedAddGuild : public ::testing::Test {
protected:
    void SetUp() override {
        m_GuildID = Guild::getMaxGuildID();
        m_SlayerZoneID = Guild::getMaxSlayerZoneID();
        m_VampireZoneID = Guild::getMaxVampireZoneID();
        m_OustersZoneID = Guild::getMaxOustersZoneID();
    }

    void TearDown() override {
        Guild::setMaxGuildID(m_GuildID);
        Guild::setMaxSlayerZoneID(m_SlayerZoneID);
        Guild::setMaxVampireZoneID(m_VampireZoneID);
        Guild::setMaxOustersZoneID(m_OustersZoneID);
    }

    static void found(GuildRace_t race) {
        GSAddGuild packet;
        packet.setGuildRace(race);
        packet.setGuildName("Refused founding");
        EXPECT_NO_THROW(GSAddGuildHandler::execute(&packet, nullptr));
    }

private:
    GuildID_t m_GuildID = 0;
    ZoneID_t m_SlayerZoneID = 0;
    ZoneID_t m_VampireZoneID = 0;
    ZoneID_t m_OustersZoneID = 0;
};

// The next id would wrap to zero, so nothing is founded and the counter stays full.
TEST_F(SharedAddGuild, AFullGuildIDCounterFoundsNothing) {
    Guild::setMaxGuildID(kFullGuildIDs);
    Guild::setMaxSlayerZoneID(10001);

    found(Guild::GUILD_RACE_SLAYER);

    EXPECT_EQ(Guild::getMaxGuildID(), kFullGuildIDs);
    EXPECT_EQ(Guild::getMaxSlayerZoneID(), 10001);
}

TEST_F(SharedAddGuild, ARaceWhoseNextZoneIDIsTheLastFoundsNothingAndClaimsNoGuildID) {
    Guild::setMaxGuildID(500);
    Guild::setMaxSlayerZoneID(kLastZoneID);

    found(Guild::GUILD_RACE_SLAYER);

    EXPECT_EQ(Guild::getMaxGuildID(), 500);
    EXPECT_EQ(Guild::getMaxSlayerZoneID(), kLastZoneID);
}

TEST_F(SharedAddGuild, AnUnknownRaceFoundsNothingAndClaimsNoGuildID) {
    Guild::setMaxGuildID(500);

    found(7);

    EXPECT_EQ(Guild::getMaxGuildID(), 500);
}

} // namespace
