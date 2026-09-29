//////////////////////////////////////////////////////////////////////
//
// Filename    : login_client_link_test.cpp
// Description : The loginserver's client link admits only what a client
//               sends: CL packets, and the CG packet every fresh
//               connection opens with. Compiled as the loginserver
//               (__LOGIN_SERVER__) and linked with LoginServerPackets, so
//               the factory table and the validator are the
//               loginserver's own. The GL packets the gameservers send
//               arrive on the datagram socket, never on a LoginPlayer's.
//
//////////////////////////////////////////////////////////////////////

#include <string>

#include <gtest/gtest.h>

#include "Exception.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PlayerStatus.h"

namespace {

class LoginClientLink : public ::testing::Test {
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

TEST_F(LoginClientLink, NoStatusAdmitsARegisteredPacketAClientDoesNotSend) {
    int admittedCount = 0;
    for (int s = 0; s < PLAYER_STATUS_MAX; s++) {
        const PlayerStatus status = PlayerStatus(s);
        for (PacketID_t id = 0; id < Packet::PACKET_MAX; id++) {
            const std::string name = registeredName(id);
            if (name.empty() || !admitted(status, id))
                continue;
            admittedCount++;
            const bool clientSends = name.rfind("CL", 0) == 0 || name.rfind("CG", 0) == 0;
            EXPECT_TRUE(clientSends) << "status " << s << " admits " << name << " (" << id << ")";
        }
    }
    EXPECT_GT(admittedCount, 20);
}

TEST_F(LoginClientLink, KickVerifyWaitAdmitsNoGameServerPacket) {
    EXPECT_FALSE(admitted(LPS_WAITING_FOR_GL_KICK_VERIFY, Packet::PACKET_GL_KICK_VERIFY));
}

TEST_F(LoginClientLink, TheLoginPhaseStillAdmitsTheClientsPackets) {
    EXPECT_TRUE(admitted(LPS_BEGIN_SESSION, Packet::PACKET_CL_LOGIN));
    EXPECT_TRUE(admitted(LPS_BEGIN_SESSION, Packet::PACKET_CL_VERSION_CHECK));
    EXPECT_TRUE(admitted(LPS_PC_MANAGEMENT, Packet::PACKET_CL_SELECT_PC));
}

} // namespace
