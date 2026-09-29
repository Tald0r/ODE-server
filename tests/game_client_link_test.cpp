//////////////////////////////////////////////////////////////////////
//
// Filename    : game_client_link_test.cpp
// Description : The gameserver's client link admits only what a game
//               client sends. Compiled as the gameserver (__GAME_SERVER__)
//               and linked with GameServerPackets, so the factory table
//               and the validator are the gameserver's own.
//
//               The oracle is written here from the packet names, apart
//               from the production rule in GameClientLink.h: a
//               registered packet a client sends on the game connection
//               is CG-named and not a datagram, or one of the three
//               GC-named packets the live client sends server-ward.
//
//////////////////////////////////////////////////////////////////////

#include <set>
#include <string>

#include <gtest/gtest.h>

#include "Datagram.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PlayerStatus.h"

namespace {

class GameClientLink : public ::testing::Test {
protected:
    void SetUp() override {
        m_Factories.init();
        m_Validator.init();
    }

    // The registered factory's name, or empty when the id has none: the
    // receive loop refuses such an id by its max size, before any read.
    std::string registeredName(PacketID_t id) {
        try {
            return m_Factories.getPacketName(id);
        } catch (InvalidProtocolException&) {
            return std::string();
        }
    }

    bool clientSends(PacketID_t id, const std::string& name) {
        static const std::set<std::string> kClientSentGC{"GCAddStoreItem", "GCFriendChatting", "GCRemoveStoreItem"};
        Datagram datagram;
        if (name.rfind("CG", 0) == 0)
            return !datagram.isDatagram(id);
        return kClientSentGC.count(name) != 0;
    }

    // Whether the receive loop would go on to read the packet: an ignored
    // id (PIST_IGNORE_EXCEPT) is skipped unread.
    bool admitted(PlayerStatus status, PacketID_t id) {
        try {
            return m_Validator.isValidPacketID(status, id);
        } catch (IgnorePacketException&) {
            return false;
        }
    }

    PacketFactoryManager m_Factories;
    PacketValidator m_Validator;
};

TEST_F(GameClientLink, NormalAdmitsEveryRegisteredPacketAClientSendsAndNothingElse) {
    int clientSent = 0;
    for (PacketID_t id = 0; id < Packet::PACKET_MAX; id++) {
        const std::string name = registeredName(id);
        if (name.empty()) {
            EXPECT_FALSE(admitted(GPS_NORMAL, id)) << "unregistered id " << id;
            continue;
        }
        const bool expected = clientSends(id, name);
        EXPECT_EQ(admitted(GPS_NORMAL, id), expected) << name << " (" << id << ")";
        clientSent += expected;
    }
    // The factory table holds well over a hundred CG packets; a count
    // near zero would mean the enumeration, not the validator, is wrong.
    EXPECT_GT(clientSent, 100);
}

TEST_F(GameClientLink, NormalRefusesThePacketsOnlyAServerSends) {
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_GC_UPDATE_INFO));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_GC_PET_INFO));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_GC_SHOP_LIST));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_GC_MY_STORE_INFO));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_GG_COMMAND));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_GS_GUILDMEMBER_LOGON));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_SG_GUILDMEMBER_LOGON_OK));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_LG_KICK_CHARACTER));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_GL_KICK_VERIFY));
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_CG_PORT_CHECK));
    // The client only receives GCCannotUse; it has no send of it.
    EXPECT_FALSE(admitted(GPS_NORMAL, Packet::PACKET_GC_CANNOT_USE));
}

TEST_F(GameClientLink, NormalAdmitsTheGCPacketsTheClientSends) {
    EXPECT_TRUE(admitted(GPS_NORMAL, Packet::PACKET_GC_FRIEND_CHATTING));
    EXPECT_TRUE(admitted(GPS_NORMAL, Packet::PACKET_GC_ADD_STORE_ITEM));
    EXPECT_TRUE(admitted(GPS_NORMAL, Packet::PACKET_GC_REMOVE_STORE_ITEM));
    EXPECT_TRUE(admitted(GPS_NORMAL, Packet::PACKET_CG_MOVE));
    EXPECT_TRUE(admitted(GPS_NORMAL, Packet::PACKET_CG_READY));
}

// Every other status names its packets by hand; none of them may name a
// registered packet a client does not send either.
TEST_F(GameClientLink, NoStatusAdmitsARegisteredPacketAClientDoesNotSend) {
    for (int s = 0; s < PLAYER_STATUS_MAX; s++) {
        const PlayerStatus status = PlayerStatus(s);
        for (PacketID_t id = 0; id < Packet::PACKET_MAX; id++) {
            const std::string name = registeredName(id);
            if (name.empty() || !admitted(status, id))
                continue;
            EXPECT_TRUE(clientSends(id, name)) << "status " << s << " admits " << name << " (" << id << ")";
        }
    }
}

} // namespace
