//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_interserver_test.cpp
// Description : Golden byte fixtures, round trips and size pins for
//               every packet the three server processes say to each
//               other: the gameserver/loginserver UDP link (GL, LG,
//               GG, GM) and the gameserver/sharedserver TCP link
//               (GS, SG).
//
//               Thirty packets, in four groups.
//
//               The UDP link, gameserver to loginserver (GL):
//
//               GLIncomingConnection       the game server asking the
//                                login server whether the account that
//                                just opened a TCP socket really is
//                                logging in from that address.
//               GLIncomingConnectionOK     the login server's answer
//                                when it is, carrying the port and the
//                                one-shot key the client must present.
//               GLIncomingConnectionError  the answer when it is not.
//               GLKickVerify     the game server confirming that a
//                                character named in an LGKickCharacter
//                                was, or was not, thrown off.
//
//               The same link, loginserver to gameserver (LG):
//
//               LGIncomingConnection       the login server announcing
//                                the account, character and address
//                                that is about to connect.
//               LGIncomingConnectionOK     and its acknowledgement
//               LGIncomingConnectionError  pair, the mirror image of
//                                the GL two.
//               LGKickCharacter  the login server ordering a character
//                                off a game server, the packet
//                                GLKickVerify answers.
//
//               Game server to game server over the same socket (GG):
//
//               GGCommand        an operator console line relayed to
//                                another game server.
//               GGGuildChat      one guild message fanned out to the
//                                servers a guild's members are on.
//               GGServerChat     one whisper or shout relayed the same
//                                way, with the sender's race.
//
//               And the report every game server sends the login
//               server on a timer (GM):
//
//               GMServerInfo     the world, the server and the per-zone
//                                population table the login server
//                                balances new logins against.
//
//               The TCP link, gameserver to sharedserver (GS):
//
//               GSRequestGuildInfo  the whole guild table, please: the
//                                first thing a game server asks for.
//               GSAddGuild       a guild was created here.
//               GSAddGuildMember a character joined one.
//               GSExpelGuildMember  a master threw one out.
//               GSQuitGuild      a member left of their own accord.
//               GSModifyGuildMember  a member's rank changed.
//               GSModifyGuildIntro   the guild's introduction text
//                                changed.
//               GSGuildMemberLogOn   a member logged on or off, so the
//                                other servers can grey the name.
//
//               And the sharedserver's answers (SG):
//
//               SGGuildInfo      the whole table: a list of guilds,
//                                each with its own member list. The
//                                only packet here that nests a record
//                                inside a record.
//               SGAddGuildOK     the created guild, now with the id
//                                and zone the shared server assigned.
//               SGAddGuildMemberOK    the accepted join,
//               SGExpelGuildMemberOK  expulsion,
//               SGQuitGuildOK         departure,
//               SGModifyGuildMemberOK rank change,
//               SGModifyGuildIntroOK  introduction change and
//               SGGuildMemberLogOnOK  log-on flip, each broadcast to
//                                every game server.
//               SGDeleteGuildOK  a guild disbanded.
//               SGModifyGuildOK  a guild's state changed. No GS packet
//                                asks for this one; the shared server
//                                sends it on its own.
//
//               The two links are framed differently and are pinned
//               differently.
//
//               GS and SG are ordinary stream packets, so they get the
//               same three pins as the client-facing directions: a
//               loopback round trip through the real socket and stream
//               classes comparing every getter, the body bytes against
//               tests/golden/<Name>.code0.hex, and getPacketSize()
//               against the bytes write() emits and against the
//               factory max. None of the thirty touches the encrypter,
//               so the goldens are recorded at code 0, and every
//               stream golden also asserts its bytes do not vary with
//               the code so that adopting the encrypter fails loudly.
//
//               GL, LG, GG and GM are DatagramPacket: their read() and
//               write() take a Datagram, and writing one to a socket
//               stream throws by design. They are pinned through a
//               real Datagram instead. Its header carries a MEASURED
//               body length, not the declared one, so the size pin
//               here is three-way: the size field, the datagram's
//               length and getPacketSize() must all name the same
//               count. There is no encrypt code on this path at all,
//               which is why the datagram goldens carry no per-code
//               assertion. The socket hop itself, and the frame's
//               trailing pad byte, are pinned in
//               tests/datagram_frame_test.cpp.
//
//               Fixture values are distinct per field and >= 128 in
//               every byte the width allows. GuildState_t,
//               GuildRace_t, GuildType_t, GuildMemberRank_t,
//               ServerGroupID_t, WorldID_t and Race_t are BYTE
//               typedefs rather than enums, so they carry high bytes
//               too. Two groups cannot follow the rule and say so at
//               the point of use: the bool flags, whose domain is 0
//               and 1, and the names, IP strings and dates, which each
//               packet caps well below 128 characters.
//
//               Extra goldens cover the branches one fixture cannot.
//               The four packets with an optional guild introduction
//               (GSAddGuild, GSModifyGuildIntro, SGAddGuildOK,
//               SGModifyGuildIntroOK) and the one with an optional
//               member introduction (GSAddGuildMember) get a second
//               golden with the string absent. SGGuildInfo's canonical
//               fixture carries one fully populated guild with two
//               members and one guild with no date, no introduction
//               and no members, so every branch of GuildInfo2::write()
//               runs; SGGuildInfo.empty takes the zero-guild branch.
//               GMServerInfo.nozones takes the empty zone table.
//
//               Both ends of every packet in this file are built from
//               this repository, so a fix here is free to move bytes:
//               nothing outside it parses these.
//
//               The write/read disagreements this set found are fixed,
//               and the section at the end pins what each one produces
//               now: the two GG chat packets bound the message length
//               against the message, the four packets that carry a
//               guild introduction cut it to the width their length
//               byte and factory max allow, SGGuildInfo and GuildInfo2
//               read their lists back in the order write() sent them,
//               SGGuildInfo refuses a table past the guild count its
//               factory max budgets, GuildInfo2's max counts the
//               member-count word once, and GMServerInfo::read()
//               replaces the zone table it already holds instead of
//               appending to it.
//
//////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Datagram.h"
#include "DatagramPacket.h"
#include "Exception.h"
#include "GGCommand.h"
#include "GGGuildChat.h"
#include "GGServerChat.h"
#include "GLIncomingConnection.h"
#include "GLIncomingConnectionError.h"
#include "GLIncomingConnectionOK.h"
#include "GLKickVerify.h"
#include "GMServerInfo.h"
#include "GSAddGuild.h"
#include "GSAddGuildMember.h"
#include "GSExpelGuildMember.h"
#include "GSGuildMemberLogOn.h"
#include "GSModifyGuildIntro.h"
#include "GSModifyGuildMember.h"
#include "GSQuitGuild.h"
#include "GSRequestGuildInfo.h"
#include "GuildInfo2.h"
#include "GuildMemberInfo2.h"
#include "LGIncomingConnection.h"
#include "LGIncomingConnectionError.h"
#include "LGIncomingConnectionOK.h"
#include "LGKickCharacter.h"
#include "SGAddGuildMemberOK.h"
#include "SGAddGuildOK.h"
#include "SGDeleteGuildOK.h"
#include "SGExpelGuildMemberOK.h"
#include "SGGuildInfo.h"
#include "SGGuildMemberLogOnOK.h"
#include "SGModifyGuildIntroOK.h"
#include "SGModifyGuildMemberOK.h"
#include "SGModifyGuildOK.h"
#include "SGQuitGuildOK.h"
#include "TestStreams.h"

using wiretest::expectGolden;
using wiretest::kEncryptCodeCount;
using wiretest::kEncryptCodes;
using wiretest::Loopback;
using wiretest::roundTrip;
using wiretest::writeBody;

namespace {

// The unencrypted branch. Every stream packet in this file reads and
// writes its body with plain read()/write() calls, so this is the only
// code whose bytes differ from any other; the datagram packets have no
// encrypt code at all.
const uchar kPlainCode = 0;

//////////////////////////////////////////////////////////////////////
// Datagram plumbing.
//////////////////////////////////////////////////////////////////////

// The body a real Datagram frames for `packet`, with the frame's own
// pins taken on the way past: the datagram is long enough to hold a
// header, the size field carries the measured body length, and
// getPacketSize() names that same count.
std::vector<unsigned char> datagramBody(const DatagramPacket& packet) {
    Datagram datagram;
    datagram.write(&packet);

    EXPECT_LE(szPacketHeader, datagram.getLength());
    if (datagram.getLength() < szPacketHeader)
        return std::vector<unsigned char>();

    const unsigned char* pBuffer = (const unsigned char*)datagram.getData();
    const uint measured = datagram.getLength() - szPacketHeader;

    unsigned int sizeField = 0;
    for (uint i = 0; i < szPacketSize; i++)
        sizeField |= (unsigned int)pBuffer[szPacketID + i] << (8 * i);
    EXPECT_EQ(measured, sizeField) << packet.getPacketName() << ": the datagram's length and its size field disagree";

    const unsigned char* pBody = pBuffer + szPacketID + szPacketSize;
    return std::vector<unsigned char>(pBody, pBody + measured);
}

// Let a packet parse a raw body image, exactly as the receive path does
// once it has stripped the header.
void readDatagramImage(DatagramPacket& packet, const std::vector<unsigned char>& image) {
    // setData() rejects a NULL pointer, and an empty vector's data() may
    // be one; the extra byte is never read.
    std::vector<char> bytes(image.begin(), image.end());
    bytes.push_back(0);

    Datagram datagram;
    datagram.setData(&bytes[0], (uint)image.size());
    packet.read(datagram);
}

// A BYTE length prefix followed by the string's raw bytes — the framing
// every string field on both links uses.
void appendString(std::vector<unsigned char>& image, const std::string& value) {
    image.push_back((unsigned char)value.size());
    for (size_t i = 0; i < value.size(); i++)
        image.push_back((unsigned char)value[i]);
}

//////////////////////////////////////////////////////////////////////
// The three pins every packet gets. fill() / expectEqual() are
// overloaded per packet, as in packet_login_test.cpp, so the same
// canonical instance feeds all three.
//////////////////////////////////////////////////////////////////////

#define INTERSERVER_STREAM_TESTS(Name)                                                               \
    TEST(Name##Test, roundTripsThroughLoopback) {                                                    \
        Name src;                                                                                    \
        fill(src);                                                                                   \
        Name dst;                                                                                    \
        roundTrip(src, dst, kPlainCode);                                                             \
        expectEqual(src, dst);                                                                       \
    }                                                                                                \
    TEST(Name##Test, bodyBytesMatchGolden) {                                                         \
        Name packet;                                                                                 \
        fill(packet);                                                                                \
        const std::vector<unsigned char> body = writeBody(packet, kPlainCode);                       \
        expectGolden(#Name, kPlainCode, body);                                                       \
        for (size_t i = 1; i < kEncryptCodeCount; i++)                                               \
            EXPECT_EQ(body, writeBody(packet, kEncryptCodes[i]))                                     \
                << #Name " now varies with the encrypt code — add per-code goldens";                 \
    }                                                                                                \
    TEST(Name##Test, sizeMatchesTheBytesWrittenAndFitsTheFactoryMax) {                               \
        Name packet;                                                                                 \
        fill(packet);                                                                                \
        Name##Factory factory;                                                                       \
        EXPECT_EQ((size_t)packet.getPacketSize(), writeBody(packet, kPlainCode).size())              \
            << #Name ": getPacketSize() disagrees with the bytes write() emits; writePacket() puts " \
                     "the former on the wire, so the stream never resynchronises";                   \
        EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize())                                \
            << #Name ": the body outgrows the read buffer the receiver sizes from the factory max";  \
        EXPECT_EQ(factory.getPacketID(), packet.getPacketID());                                      \
        EXPECT_EQ(factory.getPacketName(), packet.getPacketName());                                  \
    }

// The datagram twin. The round trip runs through a real Datagram rather
// than a socket stream, and there is no encrypt code to vary.
#define INTERSERVER_DATAGRAM_TESTS(Name)                                                             \
    TEST(Name##Test, roundTripsThroughADatagram) {                                                   \
        Name src;                                                                                    \
        fill(src);                                                                                   \
        Name dst;                                                                                    \
        readDatagramImage(dst, datagramBody(src));                                                   \
        expectEqual(src, dst);                                                                       \
    }                                                                                                \
    TEST(Name##Test, bodyBytesMatchGolden) {                                                         \
        Name packet;                                                                                 \
        fill(packet);                                                                                \
        expectGolden(#Name, kPlainCode, datagramBody(packet));                                       \
    }                                                                                                \
    TEST(Name##Test, sizeMatchesTheBytesWrittenAndFitsTheFactoryMax) {                               \
        Name packet;                                                                                 \
        fill(packet);                                                                                \
        Name##Factory factory;                                                                       \
        EXPECT_EQ((size_t)packet.getPacketSize(), datagramBody(packet).size())                       \
            << #Name ": getPacketSize() disagrees with the bytes write() emits; the datagram's own " \
                     "size field is measured, so the two ends disagree about the body";              \
        EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize())                                \
            << #Name ": the body outgrows the read buffer the receiver sizes from the factory max";  \
        EXPECT_EQ(factory.getPacketID(), packet.getPacketID());                                      \
        EXPECT_EQ(factory.getPacketName(), packet.getPacketName());                                  \
    }

// A second fixture for a packet whose write() has a branch the
// canonical one does not take.
#define INTERSERVER_STREAM_VARIANT(Name, Variant, fillVariant)                 \
    TEST(Name##Test, Variant##BodyBytesMatchGolden) {                          \
        Name packet;                                                           \
        fillVariant(packet);                                                   \
        const std::vector<unsigned char> body = writeBody(packet, kPlainCode); \
        expectGolden(#Name "." #Variant, kPlainCode, body);                    \
        EXPECT_EQ((size_t)packet.getPacketSize(), body.size());                \
        Name##Factory factory;                                                 \
        EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());         \
        Name dst;                                                              \
        roundTrip(packet, dst, kPlainCode);                                    \
        expectEqual(packet, dst);                                              \
    }

#define INTERSERVER_DATAGRAM_VARIANT(Name, Variant, fillVariant)       \
    TEST(Name##Test, Variant##BodyBytesMatchGolden) {                  \
        Name packet;                                                   \
        fillVariant(packet);                                           \
        const std::vector<unsigned char> body = datagramBody(packet);  \
        expectGolden(#Name "." #Variant, kPlainCode, body);            \
        EXPECT_EQ((size_t)packet.getPacketSize(), body.size());        \
        Name##Factory factory;                                         \
        EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize()); \
        Name dst;                                                      \
        readDatagramImage(dst, body);                                  \
        expectEqual(packet, dst);                                      \
    }

//////////////////////////////////////////////////////////////////////
// The UDP link: GL.
//////////////////////////////////////////////////////////////////////

void fill(GLIncomingConnection& packet) {
    packet.setPlayerID("glPlayerId");
    // The writer caps the address at 15 characters, so the fixture is a
    // full-width dotted quad rather than a high-byte string.
    packet.setClientIP("203.198.171.94");
}

void expectEqual(const GLIncomingConnection& a, const GLIncomingConnection& b) {
    EXPECT_EQ(a.getPlayerID(), b.getPlayerID());
    EXPECT_EQ(a.getClientIP(), b.getClientIP());
}

INTERSERVER_DATAGRAM_TESTS(GLIncomingConnection)

void fill(GLIncomingConnectionError& packet) {
    packet.setMessage("gl connection refused");
    packet.setPlayerID("glErrPlayer");
}

void expectEqual(const GLIncomingConnectionError& a, const GLIncomingConnectionError& b) {
    EXPECT_EQ(a.getMessage(), b.getMessage());
    EXPECT_EQ(a.getPlayerID(), b.getPlayerID());
}

INTERSERVER_DATAGRAM_TESTS(GLIncomingConnectionError)

void fill(GLIncomingConnectionOK& packet) {
    packet.setPlayerID("glOkPlayer");
    packet.setTCPPort(0x9DAE8FBC);
    packet.setKey(0xB1C2D3E4);
}

void expectEqual(const GLIncomingConnectionOK& a, const GLIncomingConnectionOK& b) {
    EXPECT_EQ(a.getPlayerID(), b.getPlayerID());
    EXPECT_EQ(a.getTCPPort(), b.getTCPPort());
    EXPECT_EQ(a.getKey(), b.getKey());
}

INTERSERVER_DATAGRAM_TESTS(GLIncomingConnectionOK)

void fill(GLKickVerify& packet) {
    // A bool on the wire: the writer narrows it to 0 or 1, so this one
    // field cannot carry a high byte.
    packet.setKicked(true);
    packet.setID(0x8A9BACBD);
    packet.setPCName("GlKickTarget");
}

void expectEqual(const GLKickVerify& a, const GLKickVerify& b) {
    EXPECT_EQ(a.isKicked(), b.isKicked());
    EXPECT_EQ(a.getID(), b.getID());
    EXPECT_EQ(a.getPCName(), b.getPCName());
}

INTERSERVER_DATAGRAM_TESTS(GLKickVerify)

//////////////////////////////////////////////////////////////////////
// The UDP link: LG.
//////////////////////////////////////////////////////////////////////

void fill(LGIncomingConnection& packet) {
    packet.setPlayerID("lgPlayerId");
    packet.setPCName("LgArrivingHero");
    // 15 characters, the widest address the writer accepts.
    packet.setClientIP("198.187.166.155");
}

void expectEqual(const LGIncomingConnection& a, const LGIncomingConnection& b) {
    EXPECT_EQ(a.getPlayerID(), b.getPlayerID());
    EXPECT_EQ(a.getPCName(), b.getPCName());
    EXPECT_EQ(a.getClientIP(), b.getClientIP());
}

INTERSERVER_DATAGRAM_TESTS(LGIncomingConnection)

void fill(LGIncomingConnectionError& packet) {
    packet.setMessage("lg connection refused");
    packet.setPlayerID("lgErrPlayer");
}

void expectEqual(const LGIncomingConnectionError& a, const LGIncomingConnectionError& b) {
    EXPECT_EQ(a.getMessage(), b.getMessage());
    EXPECT_EQ(a.getPlayerID(), b.getPlayerID());
}

INTERSERVER_DATAGRAM_TESTS(LGIncomingConnectionError)

void fill(LGIncomingConnectionOK& packet) {
    packet.setPlayerID("lgOkPlayer");
    packet.setTCPPort(0xA1B2C3D4);
    packet.setKey(0x8E9FA0B1);
}

void expectEqual(const LGIncomingConnectionOK& a, const LGIncomingConnectionOK& b) {
    EXPECT_EQ(a.getPlayerID(), b.getPlayerID());
    EXPECT_EQ(a.getTCPPort(), b.getTCPPort());
    EXPECT_EQ(a.getKey(), b.getKey());
}

INTERSERVER_DATAGRAM_TESTS(LGIncomingConnectionOK)

void fill(LGKickCharacter& packet) {
    packet.setID(0xC3D4E5F6);
    packet.setPCName("LgKickTarget");
}

void expectEqual(const LGKickCharacter& a, const LGKickCharacter& b) {
    EXPECT_EQ(a.getID(), b.getID());
    EXPECT_EQ(a.getPCName(), b.getPCName());
}

INTERSERVER_DATAGRAM_TESTS(LGKickCharacter)

// Validate the original string size, before the one-byte wire length can
// wrap. Other fields keep their ordinary fixture values so each refusal is
// attributable to this field. A writer may already have emitted earlier
// fields when it refuses; these tests do not require transactional writes.
template <typename P, typename SetField> void expectDatagramStringBounds(size_t maximum, SetField setField) {
    for (size_t length : {size_t{1}, maximum}) {
        SCOPED_TRACE(length);
        P packet;
        fill(packet);
        setField(packet, std::string(length, 'x'));
        const auto body = datagramBody(packet);
        EXPECT_EQ(static_cast<size_t>(packet.getPacketSize()), body.size());
        P decoded;
        readDatagramImage(decoded, body);
        expectEqual(packet, decoded);
    }

    for (size_t length :
         {size_t{0}, maximum + 1, size_t{255}, size_t{256}, size_t{257}, 256 + maximum, size_t{512}, size_t{513}}) {
        SCOPED_TRACE(length);
        P packet;
        fill(packet);
        setField(packet, std::string(length, 'x'));
        Datagram datagram;
        EXPECT_THROW(datagram.write(&packet), InvalidProtocolException);
    }
}

TEST(DatagramStringBounds, KickCharacterName) {
    expectDatagramStringBounds<LGKickCharacter>(20, [](auto& packet, const auto& value) { packet.setPCName(value); });
}

TEST(DatagramStringBounds, IncomingConnectionOKPlayerID) {
    expectDatagramStringBounds<GLIncomingConnectionOK>(
        20, [](auto& packet, const auto& value) { packet.setPlayerID(value); });
}

TEST(DatagramStringBounds, IncomingConnectionErrorMessage) {
    expectDatagramStringBounds<GLIncomingConnectionError>(
        127, [](auto& packet, const auto& value) { packet.setMessage(value); });
}

TEST(DatagramStringBounds, IncomingConnectionErrorPlayerID) {
    expectDatagramStringBounds<GLIncomingConnectionError>(
        127, [](auto& packet, const auto& value) { packet.setPlayerID(value); });
}

TEST(DatagramStringBounds, LoginIncomingConnectionOKPlayerID) {
    expectDatagramStringBounds<LGIncomingConnectionOK>(
        20, [](auto& packet, const auto& value) { packet.setPlayerID(value); });
}

TEST(DatagramStringBounds, LoginIncomingConnectionErrorMessage) {
    expectDatagramStringBounds<LGIncomingConnectionError>(
        127, [](auto& packet, const auto& value) { packet.setMessage(value); });
}

TEST(DatagramStringBounds, LoginIncomingConnectionErrorPlayerID) {
    expectDatagramStringBounds<LGIncomingConnectionError>(
        127, [](auto& packet, const auto& value) { packet.setPlayerID(value); });
}

TEST(DatagramStringBounds, IncomingConnectionPlayerID) {
    expectDatagramStringBounds<LGIncomingConnection>(
        20, [](auto& packet, const auto& value) { packet.setPlayerID(value); });
}

TEST(DatagramStringBounds, IncomingConnectionCharacterName) {
    expectDatagramStringBounds<LGIncomingConnection>(20,
                                                     [](auto& packet, const auto& value) { packet.setPCName(value); });
}

TEST(DatagramStringBounds, IncomingConnectionClientIP) {
    expectDatagramStringBounds<LGIncomingConnection>(
        15, [](auto& packet, const auto& value) { packet.setClientIP(value); });
}

//////////////////////////////////////////////////////////////////////
// The UDP link: GG.
//////////////////////////////////////////////////////////////////////

void fill(GGCommand& packet) {
    packet.setCommand("gg relayed console command");
}

void expectEqual(const GGCommand& a, const GGCommand& b) {
    EXPECT_EQ(a.getCommand(), b.getCommand());
}

INTERSERVER_DATAGRAM_TESTS(GGCommand)

void fill(GGGuildChat& packet) {
    packet.setType(0x8D);
    packet.setGuildID(0x9EAF);
    // The writer caps the sender at 10 characters.
    packet.setSender("GgSender01");
    packet.setColor(0xB0C1D2E3);
    packet.setMessage("gg guild chat message");
}

void expectEqual(const GGGuildChat& a, const GGGuildChat& b) {
    EXPECT_EQ((int)a.getType(), (int)b.getType());
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getSender(), b.getSender());
    EXPECT_EQ(a.getColor(), b.getColor());
    EXPECT_EQ(a.getMessage(), b.getMessage());
}

INTERSERVER_DATAGRAM_TESTS(GGGuildChat)

void fill(GGServerChat& packet) {
    // Sender and receiver are capped at 10 characters each.
    packet.setSender("GgChatter1");
    packet.setReceiver("GgTarget02");
    packet.setColor(0x84959AB7);
    packet.setMessage("gg server chat message");
    // Race_t is a BYTE typedef rather than an enum, so no enumerator
    // constraint applies and the field carries a high byte.
    packet.setRace(0x8F);
}

void expectEqual(const GGServerChat& a, const GGServerChat& b) {
    EXPECT_EQ(a.getSender(), b.getSender());
    EXPECT_EQ(a.getReceiver(), b.getReceiver());
    EXPECT_EQ(a.getColor(), b.getColor());
    EXPECT_EQ(a.getMessage(), b.getMessage());
    EXPECT_EQ((int)a.getRace(), (int)b.getRace());
}

INTERSERVER_DATAGRAM_TESTS(GGServerChat)

//////////////////////////////////////////////////////////////////////
// The UDP link: GM.
//////////////////////////////////////////////////////////////////////

void fill(GMServerInfo& packet) {
    packet.setWorldID(0x8B);
    packet.setServerID(0x9C);
    packet.addZoneUserData(0xA1B2, 0xC3D4);
    packet.addZoneUserData(0xA5B6, 0xC7D8);
    packet.addZoneUserData(0xA9BA, 0xCBDC);
}

void fillNoZones(GMServerInfo& packet) {
    packet.setWorldID(0x8B);
    packet.setServerID(0x9C);
}

// The zone table is only readable by destroying it, so comparing two
// packets empties both. Nothing reads them afterwards.
void expectEqual(GMServerInfo& a, GMServerInfo& b) {
    EXPECT_EQ((int)a.getWorldID(), (int)b.getWorldID());
    EXPECT_EQ((int)a.getServerID(), (int)b.getServerID());
    ASSERT_EQ((int)a.getZoneUserCount(), (int)b.getZoneUserCount());

    const int zones = (int)a.getZoneUserCount();
    for (int i = 0; i < zones; i++) {
        ZONEUSERDATA left, right;
        a.popZoneUserData(left);
        b.popZoneUserData(right);
        EXPECT_EQ(left.ZoneID, right.ZoneID) << "zone " << i;
        EXPECT_EQ(left.UserNum, right.UserNum) << "zone " << i;
    }
}

INTERSERVER_DATAGRAM_TESTS(GMServerInfo)
INTERSERVER_DATAGRAM_VARIANT(GMServerInfo, nozones, fillNoZones)

//////////////////////////////////////////////////////////////////////
// The TCP link: GS.
//////////////////////////////////////////////////////////////////////

void fill(GSRequestGuildInfo&) {}

void expectEqual(const GSRequestGuildInfo& a, const GSRequestGuildInfo& b) {
    EXPECT_EQ(0u, a.getPacketSize());
    EXPECT_EQ(0u, b.getPacketSize());
}

INTERSERVER_STREAM_TESTS(GSRequestGuildInfo)

void fillCommon(GSAddGuild& packet) {
    packet.setGuildName("GsCreatedGuildName");
    packet.setGuildMaster("GsGuildMaster");
    packet.setGuildState(0x81);
    packet.setGuildRace(0x92);
    packet.setServerGroupID(0xA3);
}

void fill(GSAddGuild& packet) {
    fillCommon(packet);
    packet.setGuildIntro("GsGuildIntroduction");
}

void fillNoIntro(GSAddGuild& packet) {
    fillCommon(packet);
    packet.setGuildIntro("");
}

void expectEqual(const GSAddGuild& a, const GSAddGuild& b) {
    EXPECT_EQ(a.getGuildName(), b.getGuildName());
    EXPECT_EQ(a.getGuildMaster(), b.getGuildMaster());
    EXPECT_EQ(a.getGuildIntro(), b.getGuildIntro());
    EXPECT_EQ((int)a.getGuildState(), (int)b.getGuildState());
    EXPECT_EQ((int)a.getGuildRace(), (int)b.getGuildRace());
    EXPECT_EQ((int)a.getServerGroupID(), (int)b.getServerGroupID());
}

INTERSERVER_STREAM_TESTS(GSAddGuild)
INTERSERVER_STREAM_VARIANT(GSAddGuild, nointro, fillNoIntro)

void fillCommon(GSAddGuildMember& packet) {
    packet.setGuildID(0x84B5);
    packet.setName("GsJoiningMember");
    packet.setGuildMemberRank(0x96);
    packet.setServerGroupID(0xA7);
}

void fill(GSAddGuildMember& packet) {
    fillCommon(packet);
    packet.setGuildMemberIntro("GsMemberIntroduction");
}

void fillNoIntro(GSAddGuildMember& packet) {
    fillCommon(packet);
    packet.setGuildMemberIntro("");
}

void expectEqual(const GSAddGuildMember& a, const GSAddGuildMember& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ((int)a.getGuildMemberRank(), (int)b.getGuildMemberRank());
    EXPECT_EQ(a.getGuildMemberIntro(), b.getGuildMemberIntro());
    EXPECT_EQ((int)a.getServerGroupID(), (int)b.getServerGroupID());
}

INTERSERVER_STREAM_TESTS(GSAddGuildMember)
INTERSERVER_STREAM_VARIANT(GSAddGuildMember, nointro, fillNoIntro)

void fill(GSExpelGuildMember& packet) {
    packet.setGuildID(0x88B9);
    packet.setName("GsExpelledOne");
    packet.setSender("GsExpellingOne");
}

void expectEqual(const GSExpelGuildMember& a, const GSExpelGuildMember& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSender(), b.getSender());
}

INTERSERVER_STREAM_TESTS(GSExpelGuildMember)

void fill(GSGuildMemberLogOn& packet) {
    packet.setGuildID(0x8ACB);
    packet.setName("GsLogOnMember");
    // A bool on the wire; 0 and 1 are its whole domain.
    packet.setLogOn(true);
    packet.setServerID(0x9CDD);
}

void expectEqual(const GSGuildMemberLogOn& a, const GSGuildMemberLogOn& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getLogOn(), b.getLogOn());
    EXPECT_EQ(a.getServerID(), b.getServerID());
}

INTERSERVER_STREAM_TESTS(GSGuildMemberLogOn)

void fill(GSModifyGuildIntro& packet) {
    packet.setGuildID(0x8EDF);
    packet.setGuildIntro("GsModifiedIntroduction");
}

void fillNoIntro(GSModifyGuildIntro& packet) {
    packet.setGuildID(0x8EDF);
    packet.setGuildIntro("");
}

void expectEqual(const GSModifyGuildIntro& a, const GSModifyGuildIntro& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getGuildIntro(), b.getGuildIntro());
}

INTERSERVER_STREAM_TESTS(GSModifyGuildIntro)
INTERSERVER_STREAM_VARIANT(GSModifyGuildIntro, nointro, fillNoIntro)

void fill(GSModifyGuildMember& packet) {
    packet.setGuildID(0x90E1);
    packet.setName("GsModifiedOne");
    packet.setGuildMemberRank(0xA2);
    packet.setSender("GsModifyingOne");
}

void expectEqual(const GSModifyGuildMember& a, const GSModifyGuildMember& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ((int)a.getGuildMemberRank(), (int)b.getGuildMemberRank());
    EXPECT_EQ(a.getSender(), b.getSender());
}

INTERSERVER_STREAM_TESTS(GSModifyGuildMember)

void fill(GSQuitGuild& packet) {
    packet.setGuildID(0x92E3);
    packet.setName("GsQuittingOne");
}

void expectEqual(const GSQuitGuild& a, const GSQuitGuild& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
}

INTERSERVER_STREAM_TESTS(GSQuitGuild)

//////////////////////////////////////////////////////////////////////
// The TCP link: SG.
//////////////////////////////////////////////////////////////////////

void fillCommon(SGAddGuildOK& packet) {
    packet.setGuildID(0x94E5);
    packet.setGuildName("SgAcceptedGuild");
    packet.setGuildRace(0x86);
    packet.setGuildState(0x97);
    packet.setServerGroupID(0xA8);
    packet.setGuildZoneID(0xB9CA);
    packet.setGuildMaster("SgGuildMaster");
}

void fill(SGAddGuildOK& packet) {
    fillCommon(packet);
    packet.setGuildIntro("SgGuildIntroduction");
}

void fillNoIntro(SGAddGuildOK& packet) {
    fillCommon(packet);
    packet.setGuildIntro("");
}

void expectEqual(const SGAddGuildOK& a, const SGAddGuildOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getGuildName(), b.getGuildName());
    EXPECT_EQ((int)a.getGuildRace(), (int)b.getGuildRace());
    EXPECT_EQ((int)a.getGuildState(), (int)b.getGuildState());
    EXPECT_EQ((int)a.getServerGroupID(), (int)b.getServerGroupID());
    EXPECT_EQ(a.getGuildZoneID(), b.getGuildZoneID());
    EXPECT_EQ(a.getGuildMaster(), b.getGuildMaster());
    EXPECT_EQ(a.getGuildIntro(), b.getGuildIntro());
}

INTERSERVER_STREAM_TESTS(SGAddGuildOK)
INTERSERVER_STREAM_VARIANT(SGAddGuildOK, nointro, fillNoIntro)

void fill(SGAddGuildMemberOK& packet) {
    packet.setGuildID(0x96E7);
    packet.setName("SgAcceptedMember");
    packet.setGuildMemberRank(0xA9);
    packet.setServerGroupID(0xBA);
}

void expectEqual(const SGAddGuildMemberOK& a, const SGAddGuildMemberOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ((int)a.getGuildMemberRank(), (int)b.getGuildMemberRank());
    EXPECT_EQ((int)a.getServerGroupID(), (int)b.getServerGroupID());
}

INTERSERVER_STREAM_TESTS(SGAddGuildMemberOK)

void fill(SGDeleteGuildOK& packet) {
    packet.setGuildID(0x98E9);
}

void expectEqual(const SGDeleteGuildOK& a, const SGDeleteGuildOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
}

INTERSERVER_STREAM_TESTS(SGDeleteGuildOK)

void fill(SGExpelGuildMemberOK& packet) {
    packet.setGuildID(0x9AEB);
    packet.setName("SgExpelledOne");
    packet.setSender("SgExpellingOne");
}

void expectEqual(const SGExpelGuildMemberOK& a, const SGExpelGuildMemberOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSender(), b.getSender());
}

INTERSERVER_STREAM_TESTS(SGExpelGuildMemberOK)

void fill(SGGuildMemberLogOnOK& packet) {
    packet.setGuildID(0x9CED);
    packet.setName("SgLogOnMember");
    packet.setLogOn(true);
    packet.setServerID(0xAEBF);
}

void expectEqual(const SGGuildMemberLogOnOK& a, const SGGuildMemberLogOnOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getLogOn(), b.getLogOn());
    EXPECT_EQ(a.getServerID(), b.getServerID());
}

INTERSERVER_STREAM_TESTS(SGGuildMemberLogOnOK)

void fill(SGModifyGuildIntroOK& packet) {
    packet.setGuildID(0x9EEF);
    packet.setGuildIntro("SgModifiedIntroduction");
}

void fillNoIntro(SGModifyGuildIntroOK& packet) {
    packet.setGuildID(0x9EEF);
    packet.setGuildIntro("");
}

void expectEqual(const SGModifyGuildIntroOK& a, const SGModifyGuildIntroOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getGuildIntro(), b.getGuildIntro());
}

INTERSERVER_STREAM_TESTS(SGModifyGuildIntroOK)
INTERSERVER_STREAM_VARIANT(SGModifyGuildIntroOK, nointro, fillNoIntro)

void fill(SGModifyGuildMemberOK& packet) {
    packet.setGuildID(0xA0F1);
    packet.setName("SgModifiedOne");
    packet.setGuildMemberRank(0xB2);
    packet.setSender("SgModifyingOne");
}

void expectEqual(const SGModifyGuildMemberOK& a, const SGModifyGuildMemberOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ((int)a.getGuildMemberRank(), (int)b.getGuildMemberRank());
    EXPECT_EQ(a.getSender(), b.getSender());
}

INTERSERVER_STREAM_TESTS(SGModifyGuildMemberOK)

void fill(SGModifyGuildOK& packet) {
    packet.setGuildID(0xA2F3);
    packet.setGuildState(0xB4);
}

void expectEqual(const SGModifyGuildOK& a, const SGModifyGuildOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ((int)a.getGuildState(), (int)b.getGuildState());
}

INTERSERVER_STREAM_TESTS(SGModifyGuildOK)

void fill(SGQuitGuildOK& packet) {
    packet.setGuildID(0xA4F5);
    packet.setName("SgQuittingOne");
}

void expectEqual(const SGQuitGuildOK& a, const SGQuitGuildOK& b) {
    EXPECT_EQ(a.getGuildID(), b.getGuildID());
    EXPECT_EQ(a.getName(), b.getName());
}

INTERSERVER_STREAM_TESTS(SGQuitGuildOK)

//////////////////////////////////////////////////////////////////////
// SGGuildInfo: the guild table, a list of guilds each carrying a list
// of members. Both lists are owning, so every fixture allocates fresh
// records and the packet frees them.
//////////////////////////////////////////////////////////////////////

GuildMemberInfo2* makeMemberInfo(GuildID_t guildID, const std::string& name, GuildMemberRank_t rank, bool logOn) {
    GuildMemberInfo2* pMember = new GuildMemberInfo2();
    pMember->setGuildID(guildID);
    pMember->setName(name);
    pMember->setRank(rank);
    // A bool on the wire; 0 and 1 are its whole domain.
    pMember->setLogOn(logOn);
    return pMember;
}

// The fully populated guild: a date, an introduction and two members,
// so every optional branch of GuildInfo2::write() is taken.
GuildInfo2* makeFullGuildInfo() {
    GuildInfo2* pGuild = new GuildInfo2();
    pGuild->setID(0xA6F7);
    pGuild->setName("InfoGuildName");
    pGuild->setType(0x88);
    pGuild->setRace(0x99);
    pGuild->setState(0xAA);
    pGuild->setServerGroupID(0xBB);
    pGuild->setZoneID(0xCCDD);
    pGuild->setMaster("InfoGuildMaster");
    // The writer caps the expiry date at 11 characters.
    pGuild->setDate("2031-12-24");
    pGuild->setIntro("InfoGuildIntroduction");
    pGuild->addGuildMemberInfo(makeMemberInfo(0xA6F7, "InfoMemberTwo", 0x9D, false));
    pGuild->addGuildMemberInfo(makeMemberInfo(0xA6F7, "InfoMemberOne", 0x8C, true));
    return pGuild;
}

// The same record with every optional field absent.
GuildInfo2* makeBareGuildInfo() {
    GuildInfo2* pGuild = new GuildInfo2();
    pGuild->setID(0xA8F9);
    pGuild->setName("BareGuildName");
    pGuild->setType(0x8E);
    pGuild->setRace(0x9F);
    pGuild->setState(0xB0);
    pGuild->setServerGroupID(0xC1);
    pGuild->setZoneID(0xD2E3);
    pGuild->setMaster("BareGuildMaster");
    pGuild->setDate("");
    pGuild->setIntro("");
    return pGuild;
}

// A guild with nothing optional and no members, for the entry-count
// finding below.
GuildInfo2* makeMinimalGuildInfo(int index) {
    GuildInfo2* pGuild = new GuildInfo2();
    pGuild->setID((GuildID_t)(0x8000 + index));
    pGuild->setName("MinimalGuild");
    pGuild->setType(0x80);
    pGuild->setRace(0x81);
    pGuild->setState(0x82);
    pGuild->setServerGroupID(0x83);
    pGuild->setZoneID(0x8485);
    pGuild->setMaster("MinimalMaster");
    pGuild->setDate("");
    pGuild->setIntro("");
    return pGuild;
}

void fill(SGGuildInfo& packet) {
    // addGuildInfo() pushes to the front, so the bare guild goes in
    // first and write() emits the full one ahead of it.
    packet.addGuildInfo(makeBareGuildInfo());
    packet.addGuildInfo(makeFullGuildInfo());
}

void fillEmpty(SGGuildInfo&) {}

void expectMemberEqual(GuildInfo2& a, GuildInfo2& b) {
    ASSERT_EQ(a.getGuildMemberInfoListNum(), b.getGuildMemberInfoListNum());

    const int members = (int)a.getGuildMemberInfoListNum();
    std::vector<GuildMemberInfo2*> left, right;
    for (int i = 0; i < members; i++) {
        left.push_back(a.popFrontGuildMemberInfoList());
        right.push_back(b.popFrontGuildMemberInfoList());
    }

    for (int i = 0; i < members; i++) {
        // read() rebuilds the list in the order write() emitted it.
        GuildMemberInfo2* pLeft = left[i];
        GuildMemberInfo2* pRight = right[i];
        EXPECT_EQ(pLeft->getGuildID(), pRight->getGuildID()) << "member " << i;
        EXPECT_EQ(pLeft->getName(), pRight->getName()) << "member " << i;
        EXPECT_EQ((int)pLeft->getRank(), (int)pRight->getRank()) << "member " << i;
        EXPECT_EQ(pLeft->getLogOn(), pRight->getLogOn()) << "member " << i;
        EXPECT_EQ(pLeft->getSize(), pRight->getSize()) << "member " << i;
    }

    for (int i = 0; i < members; i++) {
        delete left[i];
        delete right[i];
    }
}

void expectGuildEqual(GuildInfo2& a, GuildInfo2& b) {
    EXPECT_EQ(a.getID(), b.getID());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ((int)a.getType(), (int)b.getType());
    EXPECT_EQ((int)a.getRace(), (int)b.getRace());
    EXPECT_EQ((int)a.getState(), (int)b.getState());
    EXPECT_EQ((int)a.getServerGroupID(), (int)b.getServerGroupID());
    EXPECT_EQ(a.getZoneID(), b.getZoneID());
    EXPECT_EQ(a.getMaster(), b.getMaster());
    EXPECT_EQ(a.getDate(), b.getDate());
    EXPECT_EQ(a.getIntro(), b.getIntro());
    EXPECT_EQ(a.getSize(), b.getSize());
    expectMemberEqual(a, b);
}

// Both lists are drained by the comparison; nothing reads the packets
// afterwards.
void expectEqual(SGGuildInfo& a, SGGuildInfo& b) {
    ASSERT_EQ(a.getGuildInfoListNum(), b.getGuildInfoListNum());

    const int guilds = (int)a.getGuildInfoListNum();
    std::vector<GuildInfo2*> left, right;
    for (int i = 0; i < guilds; i++) {
        left.push_back(a.popFrontGuildInfoList());
        right.push_back(b.popFrontGuildInfoList());
    }

    // The guild list comes back in the order it was sent.
    for (int i = 0; i < guilds; i++)
        expectGuildEqual(*left[i], *right[i]);

    for (int i = 0; i < guilds; i++) {
        delete left[i];
        delete right[i];
    }
}

INTERSERVER_STREAM_TESTS(SGGuildInfo)
INTERSERVER_STREAM_VARIANT(SGGuildInfo, empty, fillEmpty)

//////////////////////////////////////////////////////////////////////
// What the fixed write/read disagreements produce now.
//////////////////////////////////////////////////////////////////////

// A declared message length past the 128 write() emits, and the
// factory max budgets, is refused. The sender is capped at 10 two
// lines above the guard, so only a guard naming the message bounds it.
TEST(GGGuildChatTest, aMessagePastTheCapIsRefused) {
    const std::string sender = "GgSender01";

    std::vector<unsigned char> head;
    head.push_back(0x8D);
    head.push_back(0xAF);
    head.push_back(0x9E);
    appendString(head, sender);
    for (int i = 0; i < 4; i++)
        head.push_back(0xC0);

    std::vector<unsigned char> oversized(head);
    appendString(oversized, std::string(200, 'm'));

    GGGuildChat packet;
    EXPECT_THROW(readDatagramImage(packet, oversized), InvalidProtocolException);

    // The longest message write() will emit still parses.
    const std::string longest(128, 'm');
    std::vector<unsigned char> atTheCap(head);
    appendString(atTheCap, longest);

    GGGuildChat accepted;
    readDatagramImage(accepted, atTheCap);
    EXPECT_EQ(longest, accepted.getMessage());
}

// The same guard in GGServerChat, whose race byte follows the message.
TEST(GGServerChatTest, aMessagePastTheCapIsRefused) {
    const std::string sender = "GgChatter1";
    const std::string receiver = "GgTarget02";

    std::vector<unsigned char> head;
    appendString(head, sender);
    appendString(head, receiver);
    for (int i = 0; i < 4; i++)
        head.push_back(0xC0);

    std::vector<unsigned char> oversized(head);
    appendString(oversized, std::string(200, 'm'));
    oversized.push_back(0x8F);

    GGServerChat packet;
    EXPECT_THROW(readDatagramImage(packet, oversized), InvalidProtocolException);

    const std::string longest(128, 'm');
    std::vector<unsigned char> atTheCap(head);
    appendString(atTheCap, longest);
    atTheCap.push_back(0x8F);

    GGServerChat accepted;
    readDatagramImage(accepted, atTheCap);
    EXPECT_EQ(longest, accepted.getMessage());
}

// The introduction is the last field of three of the four packets that
// carry one, and GSAddGuild puts its state, race and server group after
// it; `trailing` names those bytes, so the length byte is found by
// counting back from the end of the body.
void expectIntroCutToTheCap(const Packet& packet, const std::string& intro, PacketSize_t maxSize, size_t trailing,
                            const char* what) {
    EXPECT_EQ((size_t)GUILD_INTRO_MAX_LENGTH, intro.size()) << what << ": the setter cuts the string to the cap";

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    ASSERT_GT(body.size(), GUILD_INTRO_MAX_LENGTH + trailing) << what;

    EXPECT_EQ((size_t)packet.getPacketSize(), body.size()) << what;
    EXPECT_LE(packet.getPacketSize(), maxSize) << what << ": the longest introduction fits the factory max";
    EXPECT_EQ((int)GUILD_INTRO_MAX_LENGTH, (int)body[body.size() - trailing - GUILD_INTRO_MAX_LENGTH - 1])
        << what << ": the length byte carries the whole string";
}

// Every packet that carries a guild introduction cuts it to the width
// its one-byte length prefix can express and its factory max budgets,
// so no length byte wraps and no body outgrows the read buffer the
// receiver sizes from that max. The value a player types arrives inside
// the same one-byte frame in CGRegistGuild and CGModifyGuildIntro, so
// the game server never has to refuse one the client already limited.
TEST(InterserverBoundsTest, guildIntrosAreCutToTheCap) {
    const std::string tooLong(300, 'i');

    GSAddGuild addGuild;
    fillCommon(addGuild);
    addGuild.setGuildIntro(tooLong);
    expectIntroCutToTheCap(addGuild, addGuild.getGuildIntro(), GSAddGuildFactory::kMaxSize,
                           szGuildState + szGuildRace + szServerGroupID, "GSAddGuild");

    GSModifyGuildIntro modifyIntro;
    fill(modifyIntro);
    modifyIntro.setGuildIntro(tooLong);
    expectIntroCutToTheCap(modifyIntro, modifyIntro.getGuildIntro(), GSModifyGuildIntroFactory::kMaxSize, 0,
                           "GSModifyGuildIntro");

    SGAddGuildOK addGuildOK;
    fillCommon(addGuildOK);
    addGuildOK.setGuildIntro(tooLong);
    expectIntroCutToTheCap(addGuildOK, addGuildOK.getGuildIntro(), SGAddGuildOKFactory::kMaxSize, 0, "SGAddGuildOK");

    SGModifyGuildIntroOK modifyIntroOK;
    fill(modifyIntroOK);
    modifyIntroOK.setGuildIntro(tooLong);
    expectIntroCutToTheCap(modifyIntroOK, modifyIntroOK.getGuildIntro(), SGModifyGuildIntroOKFactory::kMaxSize, 0,
                           "SGModifyGuildIntroOK");

    // The nested guild record the shared server fills from the database
    // is cut the same way, and a whole guild still fits the budget
    // GuildInfo2::getMaxSize() gives it.
    GuildInfo2 guild;
    guild.setID(0xA6F7);
    guild.setName("CapGuildName");
    guild.setMaster("CapGuildMaster");
    guild.setDate("2031-12-24");
    guild.setIntro(tooLong);
    EXPECT_EQ((size_t)GUILD_INTRO_MAX_LENGTH, guild.getIntro().size());
    EXPECT_LE((uint)guild.getSize(), GuildInfo2::getMaxSize());
}

// A round trip rebuilds the guild table in the order write() emitted it
// rather than reversing it.
TEST(SGGuildInfoTest, theGuildListKeepsTheOrderWriteSent) {
    SGGuildInfo src;
    fill(src);

    SGGuildInfo dst;
    roundTrip(src, dst, kPlainCode);

    GuildInfo2* pSentFirst = src.popFrontGuildInfoList();
    GuildInfo2* pReceivedFirst = dst.popFrontGuildInfoList();
    ASSERT_TRUE(pSentFirst != NULL);
    ASSERT_TRUE(pReceivedFirst != NULL);

    EXPECT_EQ(pSentFirst->getID(), pReceivedFirst->getID());

    delete pSentFirst;
    delete pReceivedFirst;
}

// And each guild's member list with it.
TEST(SGGuildInfoTest, theMemberListKeepsTheOrderWriteSent) {
    SGGuildInfo src;
    src.addGuildInfo(makeFullGuildInfo());

    SGGuildInfo dst;
    roundTrip(src, dst, kPlainCode);

    GuildInfo2* pSent = src.popFrontGuildInfoList();
    GuildInfo2* pReceived = dst.popFrontGuildInfoList();
    ASSERT_TRUE(pSent != NULL);
    ASSERT_TRUE(pReceived != NULL);

    GuildMemberInfo2* pSentFirst = pSent->popFrontGuildMemberInfoList();
    GuildMemberInfo2* pReceivedFirst = pReceived->popFrontGuildMemberInfoList();
    ASSERT_TRUE(pSentFirst != NULL);
    ASSERT_TRUE(pReceivedFirst != NULL);

    EXPECT_EQ(pSentFirst->getName(), pReceivedFirst->getName());

    delete pSentFirst;
    delete pReceivedFirst;
    delete pSent;
    delete pReceived;
}

// SGGuildInfoFactory::kMaxSize is GuildInfo2::kMaxCount guilds' worth of
// GuildInfo2, and that number is the receiver's read buffer. The list
// refuses the entry past it on the way in, write() refuses a list that
// reached that length another way, and read() refuses a declared count
// past it before allocating a single record.
TEST(SGGuildInfoTest, aListPastTheFactoryBudgetIsRefused) {
    SGGuildInfo packet;
    for (uint i = 0; i < GuildInfo2::kMaxCount; i++)
        packet.addGuildInfo(makeMinimalGuildInfo((int)i));
    ASSERT_EQ((int)GuildInfo2::kMaxCount, (int)packet.getGuildInfoListNum());

    EXPECT_THROW(packet.addGuildInfo(makeMinimalGuildInfo(0)), InvalidProtocolException);
    EXPECT_EQ((int)GuildInfo2::kMaxCount, (int)packet.getGuildInfoListNum());

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    ASSERT_LE(2u, body.size());
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());
    EXPECT_EQ(GuildInfo2::kMaxCount, ((unsigned int)body[0] | ((unsigned int)body[1] << 8)));
    EXPECT_LE(packet.getPacketSize(), SGGuildInfoFactory::kMaxSize);

    const unsigned int declared = GuildInfo2::kMaxCount + 1;
    Loopback loopback;
    loopback.setCodes(kPlainCode);
    loopback.out().write((unsigned char)(declared & 0xFF));
    loopback.out().write((unsigned char)(declared >> 8));
    loopback.pump(2);

    SGGuildInfo reader;
    EXPECT_THROW(reader.read(loopback.in()), InvalidProtocolException);
}

// GuildInfo2::getMaxSize() counts the member-count word once, the way
// write() emits it, so SGGuildInfo's factory max is what its budget of
// guilds can actually occupy.
TEST(SGGuildInfoTest, guildInfoMaxSizeCountsTheMemberCountWordOnce) {
    const uint fields = szGuildID + szBYTE + 30 + szGuildType + szGuildRace + szGuildState + szServerGroupID +
                        szZoneID + szBYTE + 20 + szBYTE + 11 + szBYTE + 256;

    EXPECT_EQ(fields + szWORD + GuildMemberInfo2::getMaxSize() * 220, GuildInfo2::getMaxSize());
    EXPECT_EQ(szWORD + GuildInfo2::getMaxSize() * GuildInfo2::kMaxCount, SGGuildInfoFactory::kMaxSize);
}

// The body carries the whole zone table, so reading a second one into
// the same packet replaces the first rather than appending to it and
// leaving the count naming fewer zones than the list holds. The receive
// path builds a fresh packet every time, so nothing reaches that state
// today.
TEST(GMServerInfoTest, readingTwiceReplacesTheZoneTable) {
    GMServerInfo src;
    fill(src);
    const std::vector<unsigned char> image = datagramBody(src);

    GMServerInfo dst;
    readDatagramImage(dst, image);
    readDatagramImage(dst, image);

    EXPECT_EQ(3, (int)dst.getZoneUserCount());
    EXPECT_EQ((size_t)dst.getPacketSize(), image.size());

    ZONEUSERDATA zone;
    dst.popZoneUserData(zone);
    EXPECT_EQ(0xA1B2, (int)zone.ZoneID);
    EXPECT_EQ(0xC3D4, (int)zone.UserNum);
}

} // namespace
