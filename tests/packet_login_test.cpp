//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_login_test.cpp
// Description : Golden byte fixtures, loopback round trips and size
//               pins for every CL and LC packet — the whole login
//               phase, from the version check to the game server
//               handoff.
//
//               These 33 packets are the only thing the client and the
//               login server say to each other before a character is
//               in a zone, and the client repo carries its own
//               hand-written copy of each. A byte that moves here
//               breaks live clients with no compile error anywhere, so
//               a failing golden is a protocol change to review, not a
//               test to silence.
//
//               None of the 33 references the encrypter — they all take
//               the plain read()/write() path — so goldens are recorded
//               at encrypt code 0 only, as CGSay and CGWhisper are in
//               packet_roundtrip_test.cpp. Recording six identical
//               files would advertise coverage that does not exist. The
//               golden test of every packet also asserts that its bytes
//               do not vary with the code, so adopting the encrypter
//               fails loudly instead of silently voiding five sixths of
//               the pin.
//
//               Each packet gets three pins (LOGIN_PACKET_TESTS):
//
//               - a loopback round trip through the real socket and
//                 stream classes, comparing every getter;
//               - the body bytes against tests/golden/<Name>.code0.hex;
//               - getPacketSize() against the byte count write()
//                 actually emits, and against the factory's
//                 getPacketMaxSize(). writePacket() puts
//                 getPacketSize() on the wire BEFORE calling write(),
//                 so a disagreement is not a wrong length, it is a
//                 stream that never resynchronises; and the receiving
//                 side sizes its read buffer from the factory max, so a
//                 body that outgrows it is a truncated packet, not a
//                 caught error.
//
//               Fixture values are distinct per field and >= 128 in
//               every byte the width allows, so a transposed pair of
//               fields or a signedness flip changes the golden. Two
//               groups of fields cannot follow that rule and say so at
//               the point of use: enum-valued bytes, whose domain is a
//               handful of small values, and the PC attributes, whose
//               getters reject anything above 2000.
//
//               Range checks on the enum bytes CLCreatePC and
//               CLSelectPC read off an unauthenticated socket live in
//               packet_field_bounds_test.cpp; what is pinned here is
//               the string framing those two share with the rest of the
//               login phase.
//
//               Three framing rules the login phase used to leave open
//               are pinned below, each as the refusal it now produces:
//               everyPCRecordRefusesAnEmptyName,
//               groupNameIsCappedAndNeverEmpty and, for both list
//               packets, refusesEntriesPastTheFactoryBudget.
//
//////////////////////////////////////////////////////////////////////

#include <bit>
#include <bitset>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "CLChangeServer.h"
#include "CLCreatePC.h"
#include "CLDeletePC.h"
#include "CLGetPCList.h"
#include "CLGetServerList.h"
#include "CLGetWorldList.h"
#include "CLLogin.h"
#include "CLLogout.h"
#include "CLQueryCharacterName.h"
#include "CLQueryPlayerID.h"
#include "CLReconnectLogin.h"
#include "CLRegisterPlayer.h"
#include "CLSelectPC.h"
#include "CLSelectServer.h"
#include "CLSelectWorld.h"
#include "CLVersionCheck.h"
#include "Exception.h"
#include "LCCreatePCError.h"
#include "LCCreatePCOK.h"
#include "LCDeletePCError.h"
#include "LCDeletePCOK.h"
#include "LCLoginError.h"
#include "LCLoginOK.h"
#include "LCPCList.h"
#include "LCQueryResultCharacterName.h"
#include "LCQueryResultPlayerID.h"
#include "LCReconnect.h"
#include "LCRegisterPlayerError.h"
#include "LCRegisterPlayerOK.h"
#include "LCSelectPCError.h"
#include "LCServerList.h"
#include "LCVersionCheckError.h"
#include "LCVersionCheckOK.h"
#include "LCWorldList.h"
#include "TestStreams.h"

using wiretest::expectGolden;
using wiretest::kEncryptCodeCount;
using wiretest::kEncryptCodes;
using wiretest::Loopback;
using wiretest::roundTrip;
using wiretest::writeBody;

namespace {

// The unencrypted branch. Every packet in this file reads and writes
// its body with plain read()/write() calls, so this is the only code
// whose bytes differ from any other.
const uchar kPlainCode = 0;

// PC attributes are WORDs but their getters reject anything above
// maxSlayerAttr / maxVampireAttr / maxOustersAttr (2000), so the high
// byte of an attribute fixture cannot be >= 128. The low byte still is,
// and the three values stay distinct.
const Attr_t kSTR = 0x0781; // 1921
const Attr_t kDEX = 0x0792; // 1938
const Attr_t kINT = 0x07A3; // 1955

// Push a raw byte image through a real loopback connection and let the
// packet read it, exactly as the login server reads a client's bytes.
void readImage(Packet& packet, const std::vector<unsigned char>& bytes) {
    Loopback loopback;
    loopback.setCodes(kPlainCode);
    loopback.out().write(reinterpret_cast<const char*>(&bytes[0]), (uint)bytes.size());
    loopback.pump((uint)bytes.size());
    packet.read(loopback.in());
}

// A BYTE length prefix followed by the string's raw bytes — the framing
// every string field in the login phase uses.
void appendString(std::vector<unsigned char>& image, const std::string& value) {
    image.push_back((unsigned char)value.size());
    for (size_t i = 0; i < value.size(); i++)
        image.push_back((unsigned char)value[i]);
}

//////////////////////////////////////////////////////////////////////
// The three pins every packet gets. fill() / expectEqual() are
// overloaded per packet, as in packet_encrypter_test.cpp, so the same
// canonical instance feeds all three.
//////////////////////////////////////////////////////////////////////

#define LOGIN_PACKET_TESTS(Name)                                                                     \
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

//////////////////////////////////////////////////////////////////////
// Bodyless packets — the request/acknowledgement half of the login
// phase. A golden of zero bytes is not a formality: these ride the same
// framed header as everything else, so a field added to one of them
// desynchronises a client that still expects an empty body.
//////////////////////////////////////////////////////////////////////

#define EMPTY_LOGIN_PACKET(Name)                     \
    void fill(Name&) {}                              \
    void expectEqual(const Name& a, const Name& b) { \
        EXPECT_EQ(0u, a.getPacketSize());            \
        EXPECT_EQ(0u, b.getPacketSize());            \
    }                                                \
    LOGIN_PACKET_TESTS(Name)

EMPTY_LOGIN_PACKET(CLGetPCList)
EMPTY_LOGIN_PACKET(CLGetServerList)
EMPTY_LOGIN_PACKET(CLGetWorldList)
EMPTY_LOGIN_PACKET(CLLogout)
EMPTY_LOGIN_PACKET(LCCreatePCOK)
EMPTY_LOGIN_PACKET(LCDeletePCOK)
EMPTY_LOGIN_PACKET(LCVersionCheckError)
EMPTY_LOGIN_PACKET(LCVersionCheckOK)

//////////////////////////////////////////////////////////////////////
// CL — client to login server
//////////////////////////////////////////////////////////////////////

void fill(CLChangeServer& p) {
    p.setServerGroupID(0x8B);
}
void expectEqual(const CLChangeServer& a, const CLChangeServer& b) {
    EXPECT_EQ(a.getServerGroupID(), b.getServerGroupID());
}
LOGIN_PACKET_TESTS(CLChangeServer)

// Slot, sex, hair style and race are enum-valued bytes with a handful
// of enumerators each, so they carry their highest valid value rather
// than a high byte. read() rejects anything past it — pinned in
// packet_field_bounds_test.cpp.
void fill(CLCreatePC& p) {
    p.setName("CreatePCFixture");
    p.setSlot(SLOT2);
    p.setSex(MALE);
    p.setHairStyle(HAIR_STYLE3);
    p.setHairColor(0x8A11);
    p.setSkinColor(0x8B22);
    p.setShirtColor(0x8C33);
    p.setShirtColor(0x8D44, SUB_COLOR);
    p.setJeansColor(0x8E55);
    p.setJeansColor(0x8F66, SUB_COLOR);
    p.setSTR(0x9A1B);
    p.setDEX(0x9B2C);
    p.setINT(0x9C3D);
    p.setRace(RACE_OUSTERS);
}
void expectEqual(const CLCreatePC& a, const CLCreatePC& b) {
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSlot(), b.getSlot());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getHairStyle(), b.getHairStyle());
    EXPECT_EQ(a.getHairColor(), b.getHairColor());
    EXPECT_EQ(a.getSkinColor(), b.getSkinColor());
    EXPECT_EQ(a.getShirtColor(), b.getShirtColor());
    EXPECT_EQ(a.getShirtColor(SUB_COLOR), b.getShirtColor(SUB_COLOR));
    EXPECT_EQ(a.getJeansColor(), b.getJeansColor());
    EXPECT_EQ(a.getJeansColor(SUB_COLOR), b.getJeansColor(SUB_COLOR));
    EXPECT_EQ(a.getSTR(), b.getSTR());
    EXPECT_EQ(a.getDEX(), b.getDEX());
    EXPECT_EQ(a.getINT(), b.getINT());
    EXPECT_EQ(a.getRace(), b.getRace());
}
LOGIN_PACKET_TESTS(CLCreatePC)

TEST(CLCreatePCTest, refusesNamesOutsideOneToTwenty) {
    CLCreatePC packet;
    fill(packet);
    SocketEncryptOutputStream oStream(NULL);

    packet.setName("");
    EXPECT_THROW(packet.write(oStream), InvalidProtocolException);

    packet.setName(std::string(21, 'x'));
    EXPECT_THROW(packet.write(oStream), InvalidProtocolException);
}

void fill(CLDeletePC& p) {
    p.setName("DeleteMe");
    p.setSlot(SLOT3);
    p.setSSN("8801011234567");
}
void expectEqual(const CLDeletePC& a, const CLDeletePC& b) {
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSlot(), b.getSlot());
    EXPECT_EQ(a.getSSN(), b.getSSN());
}
LOGIN_PACKET_TESTS(CLDeletePC)

// The SSN caps at 14 on both sides even though the factory's max size
// budgets 20 bytes for it, so the slack is unreachable.
TEST(CLDeletePCTest, refusesEmptyOrOversizedNameAndSSN) {
    SocketEncryptOutputStream oStream(NULL);

    CLDeletePC emptyName;
    fill(emptyName);
    emptyName.setName("");
    EXPECT_THROW(emptyName.write(oStream), InvalidProtocolException);

    CLDeletePC longName;
    fill(longName);
    longName.setName(std::string(21, 'x'));
    EXPECT_THROW(longName.write(oStream), InvalidProtocolException);

    CLDeletePC emptySSN;
    fill(emptySSN);
    emptySSN.setSSN("");
    EXPECT_THROW(emptySSN.write(oStream), InvalidProtocolException);

    CLDeletePC longSSN;
    fill(longSSN);
    longSSN.setSSN(std::string(15, '9'));
    EXPECT_THROW(longSSN.write(oStream), InvalidProtocolException);
}

// CLLogin has no setter for its six MAC bytes and its constructor
// leaves them indeterminate, so a default-constructed instance would
// write six bytes of whatever the allocation happened to hold — no
// golden could be recorded from it. The canonical instance is built by
// reading a crafted image instead, which is also what the login server
// does with every CLLogin it ever sees.
const unsigned char kLoginMac[6] = {0x8A, 0x9B, 0xAC, 0xBD, 0xCE, 0xDF};
const char* const kLoginID = "gold-login";
const char* const kLoginPassword = "pw-fixture-01";

void fill(CLLogin& p) {
    std::vector<unsigned char> image;
    appendString(image, kLoginID);
    appendString(image, kLoginPassword);
    for (size_t i = 0; i < 6; i++)
        image.push_back(kLoginMac[i]);
    image.push_back((unsigned char)LOGIN_MODE_WEBLOGIN);
    readImage(p, image);
}
void expectEqual(const CLLogin& a, const CLLogin& b) {
    EXPECT_EQ(a.getID(), b.getID());
    EXPECT_EQ(a.getPassword(), b.getPassword());
    EXPECT_EQ(0, std::memcmp(a.getRareMacAddress(), b.getRareMacAddress(), 6));
    EXPECT_EQ(a.isWebLogin(), b.isWebLogin());
}
LOGIN_PACKET_TESTS(CLLogin)

TEST(CLLoginTest, theFixtureCarriesTheImageItWasBuiltFrom) {
    CLLogin packet;
    fill(packet);
    EXPECT_EQ(std::string(kLoginID), packet.getID());
    EXPECT_EQ(std::string(kLoginPassword), packet.getPassword());
    EXPECT_EQ(0, std::memcmp(packet.getRareMacAddress(), kLoginMac, 6));
    EXPECT_TRUE(packet.isWebLogin());
}

TEST(CLLoginTest, refusesEmptyOrOversizedIDAndPassword) {
    SocketEncryptOutputStream oStream(NULL);

    CLLogin emptyID;
    fill(emptyID);
    emptyID.setID("");
    EXPECT_THROW(emptyID.write(oStream), InvalidProtocolException);

    CLLogin longID;
    fill(longID);
    longID.setID(std::string(31, 'x'));
    EXPECT_THROW(longID.write(oStream), InvalidProtocolException);

    CLLogin emptyPassword;
    fill(emptyPassword);
    emptyPassword.setPassword("");
    EXPECT_THROW(emptyPassword.write(oStream), InvalidProtocolException);

    CLLogin longPassword;
    fill(longPassword);
    longPassword.setPassword(std::string(31, 'x'));
    EXPECT_THROW(longPassword.write(oStream), InvalidProtocolException);
}

void fill(CLQueryCharacterName& p) {
    p.setCharacterName("QueryCharName");
}
void expectEqual(const CLQueryCharacterName& a, const CLQueryCharacterName& b) {
    EXPECT_EQ(a.getCharacterName(), b.getCharacterName());
}
LOGIN_PACKET_TESTS(CLQueryCharacterName)

TEST(CLQueryCharacterNameTest, refusesNamesOutsideOneToTwenty) {
    SocketEncryptOutputStream oStream(NULL);

    CLQueryCharacterName empty;
    empty.setCharacterName("");
    EXPECT_THROW(empty.write(oStream), InvalidProtocolException);

    CLQueryCharacterName tooLong;
    tooLong.setCharacterName(std::string(21, 'x'));
    EXPECT_THROW(tooLong.write(oStream), InvalidProtocolException);
}

void fill(CLQueryPlayerID& p) {
    p.setPlayerID("queryplayer");
}
void expectEqual(const CLQueryPlayerID& a, const CLQueryPlayerID& b) {
    EXPECT_EQ(a.getPlayerID(), b.getPlayerID());
}
LOGIN_PACKET_TESTS(CLQueryPlayerID)

TEST(CLQueryPlayerIDTest, refusesIDsOutsideOneToTwenty) {
    SocketEncryptOutputStream oStream(NULL);

    CLQueryPlayerID empty;
    empty.setPlayerID("");
    EXPECT_THROW(empty.write(oStream), InvalidProtocolException);

    CLQueryPlayerID tooLong;
    tooLong.setPlayerID(std::string(21, 'x'));
    EXPECT_THROW(tooLong.write(oStream), InvalidProtocolException);
}

// The login mode byte has no general setter — setWebLogin() names the
// only value a constructed instance can put on the wire on purpose, and
// the constructor leaves the member indeterminate otherwise, so the
// fixture always calls it.
void fill(CLReconnectLogin& p) {
    p.setKey(0x8A9BACBD);
    p.setWebLogin();
}
void expectEqual(const CLReconnectLogin& a, const CLReconnectLogin& b) {
    EXPECT_EQ(a.getKey(), b.getKey());
    EXPECT_EQ(a.isWebLogin(), b.isWebLogin());
}
LOGIN_PACKET_TESTS(CLReconnectLogin)

// Every setter here truncates to the field's cap, so an over-long value
// cannot reach write(); the reachable refusals are the empty and the
// too-short ones.
void fill(CLRegisterPlayer& p) {
    p.setID("goldid");
    p.setPassword("goldpw1234");
    p.setName("RegisterFixture");
    p.setSex(FEMALE);
    p.setSSN("7001012345678");
    p.setTelephone("02-1234-5678");
    p.setCellular("010-9876-5432");
    p.setZipCode("1234567");
    p.setAddress("12 Gold Street, Elcastle");
    p.setNation(JAPAN);
    p.setEmail("gold@example.com");
    p.setHomepage("http://example.com/gold");
    p.setProfile("pinned by the login packet suite");
    p.setPublic(true);
}
void expectEqual(const CLRegisterPlayer& a, const CLRegisterPlayer& b) {
    EXPECT_EQ(a.getID(), b.getID());
    EXPECT_EQ(a.getPassword(), b.getPassword());
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getSSN(), b.getSSN());
    EXPECT_EQ(a.getTelephone(), b.getTelephone());
    EXPECT_EQ(a.getCellular(), b.getCellular());
    EXPECT_EQ(a.getZipCode(), b.getZipCode());
    EXPECT_EQ(a.getAddress(), b.getAddress());
    EXPECT_EQ(a.getNation(), b.getNation());
    EXPECT_EQ(a.getEmail(), b.getEmail());
    EXPECT_EQ(a.getHomepage(), b.getHomepage());
    EXPECT_EQ(a.getProfile(), b.getProfile());
    EXPECT_EQ(a.getPublic(), b.getPublic());
}
LOGIN_PACKET_TESTS(CLRegisterPlayer)

TEST(CLRegisterPlayerTest, refusesEmptyAndTooShortCredentials) {
    SocketEncryptOutputStream oStream(NULL);

    CLRegisterPlayer emptyID;
    fill(emptyID);
    emptyID.setID("");
    EXPECT_THROW(emptyID.write(oStream), InvalidProtocolException);

    CLRegisterPlayer shortID;
    fill(shortID);
    shortID.setID("abc");
    EXPECT_THROW(shortID.write(oStream), InvalidProtocolException);

    CLRegisterPlayer shortPassword;
    fill(shortPassword);
    shortPassword.setPassword("abcde");
    EXPECT_THROW(shortPassword.write(oStream), InvalidProtocolException);

    CLRegisterPlayer emptyProfile;
    fill(emptyProfile);
    emptyProfile.setProfile("");
    EXPECT_THROW(emptyProfile.write(oStream), InvalidProtocolException);
}

void fill(CLSelectPC& p) {
    p.setPCName("SelectMeNow");
    p.setPCType(PC_OUSTERS);
}
void expectEqual(const CLSelectPC& a, const CLSelectPC& b) {
    EXPECT_EQ(a.getPCName(), b.getPCName());
    EXPECT_EQ(a.getPCType(), b.getPCType());
}
LOGIN_PACKET_TESTS(CLSelectPC)

TEST(CLSelectPCTest, refusesNamesOutsideOneToTwentyAndUnknownPCTypes) {
    SocketEncryptOutputStream oStream(NULL);

    CLSelectPC empty;
    fill(empty);
    empty.setPCName("");
    EXPECT_THROW(empty.write(oStream), InvalidProtocolException);

    CLSelectPC tooLong;
    fill(tooLong);
    tooLong.setPCName(std::string(21, 'x'));
    EXPECT_THROW(tooLong.write(oStream), InvalidProtocolException);

    // PC_OUSTERS + 1 is still inside the two bits PCType's enumerators
    // need, so the member holds a representable value and write() gets
    // to compare it — a larger byte would have to arrive through read(),
    // which is where packet_field_bounds_test.cpp pins it.
    CLSelectPC unknownType;
    fill(unknownType);
    unknownType.setPCType((PCType)(PC_OUSTERS + 1));
    EXPECT_THROW(unknownType.write(oStream), InvalidProtocolException);
}

void fill(CLSelectServer& p) {
    p.setServerGroupID(0x9C);
}
void expectEqual(const CLSelectServer& a, const CLSelectServer& b) {
    EXPECT_EQ(a.getServerGroupID(), b.getServerGroupID());
}
LOGIN_PACKET_TESTS(CLSelectServer)

void fill(CLSelectWorld& p) {
    p.setWorldID(0x8D);
}
void expectEqual(const CLSelectWorld& a, const CLSelectWorld& b) {
    EXPECT_EQ(a.getWorldID(), b.getWorldID());
}
LOGIN_PACKET_TESTS(CLSelectWorld)

void fill(CLVersionCheck& p) {
    p.setVersion(0x8A9BACBD);
}
void expectEqual(const CLVersionCheck& a, const CLVersionCheck& b) {
    EXPECT_EQ(a.getVersion(), b.getVersion());
}
LOGIN_PACKET_TESTS(CLVersionCheck)

//////////////////////////////////////////////////////////////////////
// LC — login server to client
//////////////////////////////////////////////////////////////////////

void fill(LCCreatePCError& p) {
    p.setErrorID(0x8A);
}
void expectEqual(const LCCreatePCError& a, const LCCreatePCError& b) {
    EXPECT_EQ(a.getErrorID(), b.getErrorID());
}
LOGIN_PACKET_TESTS(LCCreatePCError)

void fill(LCDeletePCError& p) {
    p.setErrorID(0x8B);
}
void expectEqual(const LCDeletePCError& a, const LCDeletePCError& b) {
    EXPECT_EQ(a.getErrorID(), b.getErrorID());
}
LOGIN_PACKET_TESTS(LCDeletePCError)

void fill(LCLoginError& p) {
    p.setErrorID(0x8C);
}
void expectEqual(const LCLoginError& a, const LCLoginError& b) {
    EXPECT_EQ(a.getErrorID(), b.getErrorID());
}
LOGIN_PACKET_TESTS(LCLoginError)

// The two flags carry opposite values so a swapped pair of bools moves
// the golden; a bool only ever puts 0 or 1 on the wire, so neither can
// follow the high-byte rule.
void fill(LCLoginOK& p) {
    p.setAdult(true);
    p.setFamily(false);
    p.setStat(0x8D);
    p.setLastDays(0x8E9F);
}
void expectEqual(const LCLoginOK& a, const LCLoginOK& b) {
    EXPECT_EQ(a.isAdult(), b.isAdult());
    EXPECT_EQ(a.isFamily(), b.isFamily());
    EXPECT_EQ(a.getStat(), b.getStat());
    EXPECT_EQ(a.getLastDays(), b.getLastDays());
}
LOGIN_PACKET_TESTS(LCLoginOK)

void fill(LCQueryResultCharacterName& p) {
    p.setCharacterName("ResultCharName");
    p.setExist(true);
}
void expectEqual(const LCQueryResultCharacterName& a, const LCQueryResultCharacterName& b) {
    EXPECT_EQ(a.getCharacterName(), b.getCharacterName());
    EXPECT_EQ(a.isExist(), b.isExist());
}
LOGIN_PACKET_TESTS(LCQueryResultCharacterName)

TEST(LCQueryResultCharacterNameTest, refusesNamesOutsideOneToTwenty) {
    SocketEncryptOutputStream oStream(NULL);

    LCQueryResultCharacterName empty;
    empty.setCharacterName("");
    empty.setExist(true);
    EXPECT_THROW(empty.write(oStream), InvalidProtocolException);

    LCQueryResultCharacterName tooLong;
    tooLong.setCharacterName(std::string(21, 'x'));
    tooLong.setExist(true);
    EXPECT_THROW(tooLong.write(oStream), InvalidProtocolException);
}

// The false flag is the point: the answer to a name query is usually
// "no such player", and a copy that skips the byte when the flag is
// false round-trips against itself while shifting nothing but the
// client's answer.
void fill(LCQueryResultPlayerID& p) {
    p.setPlayerID("resultplayer");
    p.setExist(false);
}
void expectEqual(const LCQueryResultPlayerID& a, const LCQueryResultPlayerID& b) {
    EXPECT_EQ(a.getPlayerID(), b.getPlayerID());
    EXPECT_EQ(a.isExist(), b.isExist());
}
LOGIN_PACKET_TESTS(LCQueryResultPlayerID)

TEST(LCQueryResultPlayerIDTest, refusesIDsOutsideOneToTwenty) {
    SocketEncryptOutputStream oStream(NULL);

    LCQueryResultPlayerID empty;
    empty.setPlayerID("");
    empty.setExist(true);
    EXPECT_THROW(empty.write(oStream), InvalidProtocolException);

    LCQueryResultPlayerID tooLong;
    tooLong.setPlayerID(std::string(21, 'x'));
    tooLong.setExist(true);
    EXPECT_THROW(tooLong.write(oStream), InvalidProtocolException);
}

void fill(LCReconnect& p) {
    p.setGameServerIP("203.0.113.42");
    p.setGameServerPort(0x8A9BACBD);
    p.setKey(0xCEDFE0F1);
}
void expectEqual(const LCReconnect& a, const LCReconnect& b) {
    EXPECT_EQ(a.getGameServerIP(), b.getGameServerIP());
    EXPECT_EQ(a.getGameServerPort(), b.getGameServerPort());
    EXPECT_EQ(a.getKey(), b.getKey());
}
LOGIN_PACKET_TESTS(LCReconnect)

TEST(LCReconnectTest, refusesAddressesOutsideOneToFifteen) {
    SocketEncryptOutputStream oStream(NULL);

    LCReconnect empty;
    fill(empty);
    empty.setGameServerIP("");
    EXPECT_THROW(empty.write(oStream), InvalidProtocolException);

    LCReconnect tooLong;
    fill(tooLong);
    tooLong.setGameServerIP("255.255.255.2555");
    EXPECT_THROW(tooLong.write(oStream), InvalidProtocolException);
}

void fill(LCRegisterPlayerError& p) {
    p.setErrorID(0x8E);
}
void expectEqual(const LCRegisterPlayerError& a, const LCRegisterPlayerError& b) {
    EXPECT_EQ(a.getErrorID(), b.getErrorID());
}
LOGIN_PACKET_TESTS(LCRegisterPlayerError)

void fill(LCRegisterPlayerOK& p) {
    p.setGroupName("GoldGroup");
    p.setAdult(true);
}
void expectEqual(const LCRegisterPlayerOK& a, const LCRegisterPlayerOK& b) {
    EXPECT_EQ(a.getGroupName(), b.getGroupName());
    EXPECT_EQ(a.isAdult(), b.isAdult());
}
LOGIN_PACKET_TESTS(LCRegisterPlayerOK)

// The group name is bounded on both sides, like every other string field
// in the login phase. The setter truncates, so a name of any length
// reaches the wire inside the width a BYTE prefix and the factory max
// allow; the empty name, which the stream's own string read rejects, is
// refused before any bytes are produced rather than sent undeliverable.
TEST(LCRegisterPlayerOKTest, groupNameIsCappedAndNeverEmpty) {
    SocketEncryptOutputStream oStream(NULL);
    LCRegisterPlayerOKFactory factory;

    LCRegisterPlayerOK capped;
    capped.setGroupName(std::string(256, 'g'));
    capped.setAdult(true);
    EXPECT_EQ(std::string(maxNameLength, 'g'), capped.getGroupName());

    const std::vector<unsigned char> body = writeBody(capped, kPlainCode);
    ASSERT_EQ((size_t)capped.getPacketSize(), body.size());
    EXPECT_EQ((unsigned char)maxNameLength, body[0]);
    EXPECT_LE(capped.getPacketSize(), factory.getPacketMaxSize());

    LCRegisterPlayerOK dst;
    roundTrip(capped, dst, kPlainCode);
    expectEqual(capped, dst);

    LCRegisterPlayerOK empty;
    empty.setGroupName("");
    empty.setAdult(true);
    EXPECT_THROW(empty.write(oStream), InvalidProtocolException);

    // A length byte past the cap cannot arrive from a peer either.
    std::vector<unsigned char> image;
    appendString(image, std::string(maxNameLength + 1, 'g'));
    image.push_back(1);
    LCRegisterPlayerOK oversized;
    EXPECT_THROW(readImage(oversized, image), InvalidProtocolException);
}

void fill(LCSelectPCError& p) {
    p.setCode(0x8F);
}
void expectEqual(const LCSelectPCError& a, const LCSelectPCError& b) {
    EXPECT_EQ(a.getCode(), b.getCode());
}
LOGIN_PACKET_TESTS(LCSelectPCError)

//////////////////////////////////////////////////////////////////////
// The three list packets
//////////////////////////////////////////////////////////////////////

ServerGroupInfo* makeServerGroupInfo(ServerGroupID_t id, const std::string& name, BYTE stat) {
    ServerGroupInfo* pInfo = new ServerGroupInfo();
    pInfo->setGroupID(id);
    pInfo->setGroupName(name);
    pInfo->setStat(stat);
    return pInfo;
}

// Two entries of different name lengths, so a reader that assumed a
// fixed stride lands mid-record on the second one.
void fill(LCServerList& p) {
    p.setCurrentServerGroupID(0x9A);
    p.addListElement(makeServerGroupInfo(0x8A, "Server Alpha", 0x8B));
    p.addListElement(makeServerGroupInfo(0x8C, "Beta", 0x8D));
}
void expectEqual(LCServerList& a, LCServerList& b) {
    EXPECT_EQ(a.getCurrentServerGroupID(), b.getCurrentServerGroupID());
    ASSERT_EQ(a.getListNum(), b.getListNum());
    const BYTE count = a.getListNum();
    for (BYTE i = 0; i < count; i++) {
        ServerGroupInfo* pLeft = a.popFrontListElement();
        ServerGroupInfo* pRight = b.popFrontListElement();
        EXPECT_EQ(pLeft->getGroupID(), pRight->getGroupID());
        EXPECT_EQ(pLeft->getGroupName(), pRight->getGroupName());
        EXPECT_EQ(pLeft->getStat(), pRight->getStat());
        delete pLeft;
        delete pRight;
    }
}
LOGIN_PACKET_TESTS(LCServerList)

WorldInfo* makeWorldInfo(WorldID_t id, const std::string& name, BYTE stat) {
    WorldInfo* pInfo = new WorldInfo();
    pInfo->setID(id);
    pInfo->setName(name);
    pInfo->setStat(stat);
    return pInfo;
}

void fill(LCWorldList& p) {
    p.setCurrentWorldID(0x9B);
    p.addListElement(makeWorldInfo(0x8E, "World One", 0x8F));
    p.addListElement(makeWorldInfo(0x90, "Second World Name", 0x91));
}
void expectEqual(LCWorldList& a, LCWorldList& b) {
    EXPECT_EQ(a.getCurrentWorldID(), b.getCurrentWorldID());
    ASSERT_EQ(a.getListNum(), b.getListNum());
    const BYTE count = a.getListNum();
    for (BYTE i = 0; i < count; i++) {
        WorldInfo* pLeft = a.popFrontListElement();
        WorldInfo* pRight = b.popFrontListElement();
        EXPECT_EQ(pLeft->getID(), pRight->getID());
        EXPECT_EQ(pLeft->getName(), pRight->getName());
        EXPECT_EQ(pLeft->getStat(), pRight->getStat());
        delete pLeft;
        delete pRight;
    }
}
LOGIN_PACKET_TESTS(LCWorldList)

// The entry count of both list packets comes from the login server's
// configuration, not from anything the client sends, so the cap has to
// live at fill time: a full list of full-width entries is exactly the
// factory max, and one more entry is refused instead of outgrowing the
// read buffer the receiver sizes from that max. The record's own name is
// truncated to the width the max budgets for it.
TEST(LCWorldListTest, refusesEntriesPastTheFactoryBudget) {
    LCWorldListFactory factory;
    const std::string maxName(maxNameLength, 'w');

    WorldInfo overlongName;
    overlongName.setName(std::string(64, 'w'));
    EXPECT_EQ(maxName, overlongName.getName());

    LCWorldList exactFit;
    exactFit.setCurrentWorldID(1);
    for (uint i = 0; i < WorldInfo::kMaxCount; i++)
        exactFit.addListElement(makeWorldInfo((WorldID_t)(i + 1), maxName, 0x8F));
    EXPECT_EQ(factory.getPacketMaxSize(), exactFit.getPacketSize())
        << "a full list of full-width entries is exactly the factory max";

    EXPECT_THROW(exactFit.addListElement(makeWorldInfo(0x99, maxName, 0x8F)), InvalidProtocolException);
    EXPECT_EQ((BYTE)WorldInfo::kMaxCount, exactFit.getListNum());
    EXPECT_EQ(factory.getPacketMaxSize(), exactFit.getPacketSize());
}

TEST(LCServerListTest, refusesEntriesPastTheFactoryBudget) {
    LCServerListFactory factory;
    const std::string maxName(maxNameLength, 's');

    ServerGroupInfo overlongName;
    overlongName.setGroupName(std::string(64, 's'));
    EXPECT_EQ(maxName, overlongName.getGroupName());

    LCServerList exactFit;
    exactFit.setCurrentServerGroupID(1);
    for (uint i = 0; i < ServerGroupInfo::kMaxCount; i++)
        exactFit.addListElement(makeServerGroupInfo((ServerGroupID_t)(i + 1), maxName, 0x8B));
    EXPECT_EQ(factory.getPacketMaxSize(), exactFit.getPacketSize())
        << "a full list of full-width entries is exactly the factory max";

    EXPECT_THROW(exactFit.addListElement(makeServerGroupInfo(0x99, maxName, 0x8B)), InvalidProtocolException);
    EXPECT_EQ((BYTE)ServerGroupInfo::kMaxCount, exactFit.getListNum());
    EXPECT_EQ(factory.getPacketMaxSize(), exactFit.getPacketSize());
}

//////////////////////////////////////////////////////////////////////
// LCPCList — the character selection screen: one record per slot, of a
// different shape per race.
//////////////////////////////////////////////////////////////////////

PCSlayerInfo* makeSlayerInfo() {
    PCSlayerInfo* pInfo = new PCSlayerInfo();
    pInfo->setName("GoldSlayer");
    pInfo->setSlot(SLOT1);
    pInfo->setAlignment((Alignment_t)0x8A9BACBD);
    pInfo->setSTR(kSTR);
    pInfo->setDEX(kDEX);
    pInfo->setINT(kINT);
    pInfo->setRank(0x8B);
    pInfo->setSTRExp(0x8C9DAEBF);
    pInfo->setDEXExp(0x8D9EAFC0);
    pInfo->setINTExp(0x8E9FB0C1);
    pInfo->setHP(0x8A1B, 0x8B2C);
    pInfo->setMP(0x8C3D, 0x8D4E);
    pInfo->setFame(0x8FA0B1C2);
    for (int i = 0; i < SKILL_DOMAIN_VAMPIRE; i++)
        pInfo->setSkillDomainLevel((SkillDomain)i, (SkillLevel_t)(0x80 + i));
    pInfo->setSex(MALE);
    pInfo->setHairStyle(HAIR_STYLE3);
    pInfo->setHelmetType(HELMET3);
    pInfo->setJacketType(JACKET4);
    pInfo->setPantsType(PANTS4);
    // A cross (15), the widest weapon that needs no extension code; a mace
    // has its own fixture (maceSlayerBodyBytesMatchGolden).
    pInfo->setWeaponType(WEAPON_CROSS);
    pInfo->setShieldType(SHIELD2);
    pInfo->setHairColor(0x8A11);
    pInfo->setSkinColor(0x8B22);
    pInfo->setHelmetColor(0x8C33);
    pInfo->setJacketColor(0x8D44);
    pInfo->setPantsColor(0x8E55);
    pInfo->setWeaponColor(0x8F66);
    pInfo->setShieldColor(0x9077);
    pInfo->setAdvancementLevel(0x8D);
    return pInfo;
}

void expectEqual(const PCSlayerInfo& a, const PCSlayerInfo& b) {
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSlot(), b.getSlot());
    EXPECT_EQ(a.getAlignment(), b.getAlignment());
    EXPECT_EQ(a.getSTR(), b.getSTR());
    EXPECT_EQ(a.getDEX(), b.getDEX());
    EXPECT_EQ(a.getINT(), b.getINT());
    EXPECT_EQ(a.getRank(), b.getRank());
    EXPECT_EQ(a.getSTRExp(), b.getSTRExp());
    EXPECT_EQ(a.getDEXExp(), b.getDEXExp());
    EXPECT_EQ(a.getINTExp(), b.getINTExp());
    EXPECT_EQ(a.getHP(ATTR_CURRENT), b.getHP(ATTR_CURRENT));
    EXPECT_EQ(a.getHP(ATTR_MAX), b.getHP(ATTR_MAX));
    EXPECT_EQ(a.getMP(ATTR_CURRENT), b.getMP(ATTR_CURRENT));
    EXPECT_EQ(a.getMP(ATTR_MAX), b.getMP(ATTR_MAX));
    EXPECT_EQ(a.getFame(), b.getFame());
    for (int i = 0; i < SKILL_DOMAIN_VAMPIRE; i++)
        EXPECT_EQ(a.getSkillDomainLevel((SkillDomain)i), b.getSkillDomainLevel((SkillDomain)i));
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getHairStyle(), b.getHairStyle());
    EXPECT_EQ(a.getHelmetType(), b.getHelmetType());
    EXPECT_EQ(a.getJacketType(), b.getJacketType());
    EXPECT_EQ(a.getPantsType(), b.getPantsType());
    EXPECT_EQ(a.getWeaponType(), b.getWeaponType());
    EXPECT_EQ(a.getShieldType(), b.getShieldType());
    EXPECT_EQ(a.getHairColor(), b.getHairColor());
    EXPECT_EQ(a.getSkinColor(), b.getSkinColor());
    EXPECT_EQ(a.getHelmetColor(), b.getHelmetColor());
    EXPECT_EQ(a.getJacketColor(), b.getJacketColor());
    EXPECT_EQ(a.getPantsColor(), b.getPantsColor());
    EXPECT_EQ(a.getWeaponColor(), b.getWeaponColor());
    EXPECT_EQ(a.getShieldColor(), b.getShieldColor());
    EXPECT_EQ(a.getAdvancementLevel(), b.getAdvancementLevel());
}

PCVampireInfo* makeVampireInfo() {
    PCVampireInfo* pInfo = new PCVampireInfo();
    pInfo->setName("GoldVampireName");
    pInfo->setSlot(SLOT2);
    pInfo->setAlignment((Alignment_t)0x8F9EADBC);
    pInfo->setSex(FEMALE);
    pInfo->setBatColor(0x9188);
    pInfo->setSkinColor(0x9299);
    // The coat type is an ItemType_t (a WORD) that read()/write() carry
    // as a single byte, so anything above 255 is lost on the wire.
    pInfo->setCoatType(VAMPIRE_COAT4);
    pInfo->setCoatColor(0x93AA);
    pInfo->setSTR(kSTR);
    pInfo->setDEX(kDEX);
    pInfo->setINT(kINT);
    pInfo->setHP(0x94BB, 0x95CC);
    pInfo->setLevel(0x96);
    pInfo->setRank(0x97);
    pInfo->setExp(0x98A9BACB);
    pInfo->setFame(0x99AABBCC);
    pInfo->setBonus(0x9ADD);
    pInfo->setAdvancementLevel(0x9B);
    return pInfo;
}

void expectEqual(const PCVampireInfo& a, const PCVampireInfo& b) {
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSlot(), b.getSlot());
    EXPECT_EQ(a.getAlignment(), b.getAlignment());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getBatColor(), b.getBatColor());
    EXPECT_EQ(a.getSkinColor(), b.getSkinColor());
    EXPECT_EQ(a.getCoatType(), b.getCoatType());
    EXPECT_EQ(a.getCoatColor(), b.getCoatColor());
    EXPECT_EQ(a.getSTR(), b.getSTR());
    EXPECT_EQ(a.getDEX(), b.getDEX());
    EXPECT_EQ(a.getINT(), b.getINT());
    EXPECT_EQ(a.getHP(ATTR_CURRENT), b.getHP(ATTR_CURRENT));
    EXPECT_EQ(a.getHP(ATTR_MAX), b.getHP(ATTR_MAX));
    EXPECT_EQ(a.getLevel(), b.getLevel());
    EXPECT_EQ(a.getRank(), b.getRank());
    EXPECT_EQ(a.getExp(), b.getExp());
    EXPECT_EQ(a.getFame(), b.getFame());
    EXPECT_EQ(a.getBonus(), b.getBonus());
    EXPECT_EQ(a.getAdvancementLevel(), b.getAdvancementLevel());
}

PCOustersInfo* makeOustersInfo() {
    PCOustersInfo* pInfo = new PCOustersInfo();
    pInfo->setName("GoldOusters");
    pInfo->setSlot(SLOT3);
    pInfo->setAlignment((Alignment_t)0x9C8DAEBF);
    pInfo->setSex(MALE);
    pInfo->setCoatColor(0x9D11);
    pInfo->setHairColor(0x9E22);
    pInfo->setArmColor(0x9F33);
    pInfo->setBootsColor(0xA044);
    // Coat and arm type share one byte: three bits and one bit.
    pInfo->setCoatType(OUSTERS_COAT4);
    pInfo->setArmType(OUSTERS_ARM_CHAKRAM);
    pInfo->setSTR(kSTR);
    pInfo->setDEX(kDEX);
    pInfo->setINT(kINT);
    pInfo->setHP(0xA155, 0xA266);
    pInfo->setMP(0xA377, 0xA488);
    pInfo->setLevel(0xA5);
    pInfo->setRank(0xA6);
    pInfo->setExp(0xA7B8C9DA);
    pInfo->setFame(0xA8B9CADB);
    pInfo->setBonus(0xA9EE);
    pInfo->setSkillBonus(0xAAFF);
    pInfo->setAdvancementLevel(0xAB);
    return pInfo;
}

void expectEqual(const PCOustersInfo& a, const PCOustersInfo& b) {
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getSlot(), b.getSlot());
    EXPECT_EQ(a.getAlignment(), b.getAlignment());
    EXPECT_EQ(a.getSex(), b.getSex());
    EXPECT_EQ(a.getCoatColor(), b.getCoatColor());
    EXPECT_EQ(a.getHairColor(), b.getHairColor());
    EXPECT_EQ(a.getArmColor(), b.getArmColor());
    EXPECT_EQ(a.getBootsColor(), b.getBootsColor());
    EXPECT_EQ(a.getCoatType(), b.getCoatType());
    EXPECT_EQ(a.getArmType(), b.getArmType());
    EXPECT_EQ(a.getSTR(), b.getSTR());
    EXPECT_EQ(a.getDEX(), b.getDEX());
    EXPECT_EQ(a.getINT(), b.getINT());
    EXPECT_EQ(a.getHP(ATTR_CURRENT), b.getHP(ATTR_CURRENT));
    EXPECT_EQ(a.getHP(ATTR_MAX), b.getHP(ATTR_MAX));
    EXPECT_EQ(a.getMP(ATTR_CURRENT), b.getMP(ATTR_CURRENT));
    EXPECT_EQ(a.getMP(ATTR_MAX), b.getMP(ATTR_MAX));
    EXPECT_EQ(a.getLevel(), b.getLevel());
    EXPECT_EQ(a.getRank(), b.getRank());
    EXPECT_EQ(a.getExp(), b.getExp());
    EXPECT_EQ(a.getFame(), b.getFame());
    EXPECT_EQ(a.getBonus(), b.getBonus());
    EXPECT_EQ(a.getSkillBonus(), b.getSkillBonus());
    EXPECT_EQ(a.getAdvancementLevel(), b.getAdvancementLevel());
}

// All three slots are filled, and with a different race each, so the
// type-tag prefix, the per-race record shapes and the slot each record
// claims are all exercised at once.
void fill(LCPCList& p) {
    p.setPCInfo(SLOT1, makeSlayerInfo());
    p.setPCInfo(SLOT2, makeVampireInfo());
    p.setPCInfo(SLOT3, makeOustersInfo());
}
void expectEqual(const LCPCList& a, const LCPCList& b) {
    for (uint slot = 0; slot < SLOT_MAX; slot++) {
        const PCInfo* pLeft = a.getPCInfo((Slot)slot);
        const PCInfo* pRight = b.getPCInfo((Slot)slot);
        ASSERT_EQ(pLeft->getPCType(), pRight->getPCType()) << "slot " << slot;
    }
    expectEqual(*dynamic_cast<const PCSlayerInfo*>(a.getPCInfo(SLOT1)),
                *dynamic_cast<const PCSlayerInfo*>(b.getPCInfo(SLOT1)));
    expectEqual(*dynamic_cast<const PCVampireInfo*>(a.getPCInfo(SLOT2)),
                *dynamic_cast<const PCVampireInfo*>(b.getPCInfo(SLOT2)));
    expectEqual(*dynamic_cast<const PCOustersInfo*>(a.getPCInfo(SLOT3)),
                *dynamic_cast<const PCOustersInfo*>(b.getPCInfo(SLOT3)));
}
LOGIN_PACKET_TESTS(LCPCList)

// The empty-slot tag is its own layout: the three type bytes are
// always written, and a slot with no character contributes nothing but
// its '0'.
TEST(LCPCListTest, emptySlotsStillOccupyTheTypePrefix) {
    LCPCList packet;
    packet.setPCInfo(SLOT2, makeVampireInfo());

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    ASSERT_GE(body.size(), (size_t)SLOT_MAX);
    EXPECT_EQ('0', body[SLOT1]);
    EXPECT_EQ('V', body[SLOT2]);
    EXPECT_EQ('0', body[SLOT3]);
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());

    LCPCList dst;
    roundTrip(packet, dst, kPlainCode);
    EXPECT_THROW(dst.getPCInfo(SLOT1), NoSuchElementException);
    EXPECT_THROW(dst.getPCInfo(SLOT3), NoSuchElementException);
    expectEqual(*dynamic_cast<const PCVampireInfo*>(packet.getPCInfo(SLOT2)),
                *dynamic_cast<const PCVampireInfo*>(dst.getPCInfo(SLOT2)));
}

// All three records refuse an empty name the same way: the exception
// leaves write() and the packet is never sent. A record that swallowed
// it instead would emit a body dozens of bytes shorter than the count
// writePacket() has already put on the wire, misframing everything the
// client reads after it.
TEST(LCPCListTest, everyPCRecordRefusesAnEmptyName) {
    SocketEncryptOutputStream oStream(NULL);

    LCPCList slayer;
    PCSlayerInfo* pSlayer = makeSlayerInfo();
    pSlayer->setName("");
    slayer.setPCInfo(SLOT1, pSlayer);
    EXPECT_THROW(slayer.write(oStream), InvalidProtocolException);
    EXPECT_THROW(writeBody(slayer, kPlainCode), InvalidProtocolException);

    LCPCList vampire;
    PCVampireInfo* pVampire = makeVampireInfo();
    pVampire->setName("");
    vampire.setPCInfo(SLOT2, pVampire);
    EXPECT_THROW(vampire.write(oStream), InvalidProtocolException);

    LCPCList ousters;
    PCOustersInfo* pOusters = makeOustersInfo();
    pOusters->setName("");
    ousters.setPCInfo(SLOT3, pOusters);
    EXPECT_THROW(ousters.write(oStream), InvalidProtocolException);
}

// A slayer record's read() lets a bad length prefix out too, so a peer
// cannot walk the rest of the record off a name that never fit.
TEST(LCPCListTest, slayerRecordRefusesABadNameLengthOnTheWire) {
    LCPCList packet;
    fill(packet);

    std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    ASSERT_GT(body.size(), (size_t)SLOT_MAX);

    // The slot-1 record starts right after the three type tags, with the
    // slayer name's length prefix.
    ASSERT_EQ((unsigned char)std::string("GoldSlayer").size(), body[SLOT_MAX]);

    body[SLOT_MAX] = 0;
    LCPCList emptyName;
    EXPECT_THROW(readImage(emptyName, body), InvalidProtocolException);

    body[SLOT_MAX] = (unsigned char)(maxNameLength + 1);
    LCPCList longName;
    EXPECT_THROW(readImage(longName, body), InvalidProtocolException);
}

//////////////////////////////////////////////////////////////////////
// The slayer outlook DWORD. Its weapon is two fields. Bits 11-14 hold
// the four-bit view: the weapon itself below 16, and above it the
// stand-in slayerWeaponListShape gives it (a cross for cross1, no weapon
// for mace and mace1). Bits 17-18, past the two shield bits at 15-16,
// hold the extension code: 0, or cross1 1, mace 2, mace1 3. A client
// that knows only the four weapon bits keeps bits 0-16 of the DWORD, so
// it reads the four-bit view and never the code; a client that knows the
// code trusts it over the four bits. No weapon value reaches the shield.
//////////////////////////////////////////////////////////////////////

// The outlook as the client receives it: the DWORD a slot-1 slayer
// record writes just before its colors and its advancement level.
DWORD slayerOutlookOnTheWire(const PCSlayerInfo& info) {
    LCPCList packet;
    packet.setPCInfo(SLOT1, new PCSlayerInfo(info));
    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    const size_t tail = szColor * PCSlayerInfo::SLAYER_COLOR_MAX + szLevel;
    EXPECT_GE(body.size(), (size_t)SLOT_MAX + szDWORD + tail);
    const size_t at = body.size() - tail - szDWORD;
    return (DWORD)body[at] | ((DWORD)body[at + 1] << 8) | ((DWORD)body[at + 2] << 16) | ((DWORD)body[at + 3] << 24);
}

// Bits 11-14 of each weapon's outlook with no other field set: for 0..15
// the weapon in the four-bit layout, and for 16..18 the clamped stand-in.
// These are the DWORDs a client that knows only four weapon bits has
// always been sent, and must go on reading.
const DWORD kFourBitWeaponDWORDs[WEAPON_MAX] = {
    0x00000000, 0x00000800, 0x00001000, 0x00001800, 0x00002000, 0x00002800, 0x00003000, 0x00003800,
    0x00004000, 0x00004800, 0x00005000, 0x00005800, 0x00006000, 0x00006800, 0x00007000, 0x00007800,
    0x00007800, // cross1: a cross
    0x00000000, // mace: no weapon
    0x00000000, // mace1: no weapon
};
// Bits 17-18 of each weapon's outlook: no code below 16, then cross1 1,
// mace 2, mace1 3.
const DWORD kExtensionDWORDs[WEAPON_MAX] = {
    0,          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00020000, // cross1: 1
    0x00040000, // mace: 2
    0x00060000, // mace1: 3
};
const DWORD kShieldDWORDs[SHIELD_MAX] = {0x00000000, 0x00008000, 0x00010000};

// What a client that knows only four weapon bits makes of an outlook:
// its read() keeps bits 0-16 in a bitset<17>, and its getters mask four
// weapon bits at 11 and two shield bits at 15.
struct FourBitReading {
    DWORD outlook;
    int weapon;
    int shield;
};
FourBitReading readAsAFourBitClient(DWORD wire) {
    const DWORD outlook = (DWORD)std::bitset<17>(wire).to_ulong();
    return {outlook, (int)((outlook >> 11) & 15), (int)((outlook >> 15) & 3)};
}

// The canonical slayer record with its outlook cleared, so the DWORD
// holds nothing but what a test sets.
PCSlayerInfo namedSlayer() {
    std::unique_ptr<PCSlayerInfo> pCanonical(makeSlayerInfo());
    PCSlayerInfo info = *pCanonical;
    Color_t colors[PCSlayerInfo::SLAYER_COLOR_MAX] = {};
    info.setShapeInfo(0, colors);
    return info;
}

TEST(PCSlayerInfoOutlook, everyWeaponAndEveryShieldReadBackInEitherOrder) {
    for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++) {
        for (int shield = SHIELD_NONE; shield < SHIELD_MAX; shield++) {
            PCSlayerInfo weaponFirst = namedSlayer();
            weaponFirst.setWeaponType((WeaponType)weapon);
            weaponFirst.setShieldType((ShieldType)shield);
            PCSlayerInfo shieldFirst = namedSlayer();
            shieldFirst.setShieldType((ShieldType)shield);
            shieldFirst.setWeaponType((WeaponType)weapon);
            for (const PCSlayerInfo* pInfo : {&weaponFirst, &shieldFirst}) {
                EXPECT_EQ(weapon, pInfo->getWeaponType()) << "weapon " << weapon << ", shield " << shield;
                EXPECT_EQ(shield, pInfo->getShieldType()) << "weapon " << weapon << ", shield " << shield;
            }

            LCPCList packet;
            packet.setPCInfo(SLOT1, new PCSlayerInfo(shieldFirst));
            LCPCList dst;
            roundTrip(packet, dst, kPlainCode);
            const PCSlayerInfo& read = *dynamic_cast<const PCSlayerInfo*>(dst.getPCInfo(SLOT1));
            EXPECT_EQ(weapon, read.getWeaponType()) << "on the wire: weapon " << weapon << ", shield " << shield;
            EXPECT_EQ(shield, read.getShieldType()) << "on the wire: weapon " << weapon << ", shield " << shield;
        }
    }
}

TEST(PCSlayerInfoOutlook, replacingAWeaponLeavesNoBitOfTheOldOne) {
    for (int from = WEAPON_NONE; from < WEAPON_MAX; from++) {
        for (int to = WEAPON_NONE; to < WEAPON_MAX; to++) {
            PCSlayerInfo info = namedSlayer();
            info.setShieldType(SHIELD2);
            info.setWeaponType((WeaponType)from);
            info.setWeaponType((WeaponType)to);
            EXPECT_EQ(to, info.getWeaponType()) << from << " -> " << to;
            EXPECT_EQ(SHIELD2, info.getShieldType()) << from << " -> " << to;
            EXPECT_EQ(kFourBitWeaponDWORDs[to] | kExtensionDWORDs[to] | kShieldDWORDs[SHIELD2],
                      slayerOutlookOnTheWire(info))
                << from << " -> " << to;
        }
    }
}

// The whole DWORD of every weapon with every shield: the four-bit view at
// 11-14, the extension code at 17-18, and nothing else. Weapons below 16
// carry no code, so their DWORD is the four-bit layout's exactly.
TEST(PCSlayerInfoOutlook, everyWeaponWritesItsFourBitViewAndItsExtensionCode) {
    for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++) {
        for (int shield = SHIELD_NONE; shield < SHIELD_MAX; shield++) {
            PCSlayerInfo info = namedSlayer();
            info.setWeaponType((WeaponType)weapon);
            info.setShieldType((ShieldType)shield);
            EXPECT_EQ(kFourBitWeaponDWORDs[weapon] | kExtensionDWORDs[weapon] | kShieldDWORDs[shield],
                      slayerOutlookOnTheWire(info))
                << "weapon " << weapon << ", shield " << shield;
        }
    }
}

// Old client, new server: a client that knows only four weapon bits
// reads bits 0-16, and they are the four-bit table's DWORD exactly,
// weapon and shield alike: below 16 the weapon, from 16 the clamped
// stand-in.
TEST(PCSlayerInfoOutlook, aFourBitClientReadsTheFourBitView) {
    for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++) {
        const int seen = weapon < 16 ? weapon : weapon == WEAPON_CROSS1 ? (int)WEAPON_CROSS : (int)WEAPON_NONE;
        for (int shield = SHIELD_NONE; shield < SHIELD_MAX; shield++) {
            PCSlayerInfo info = namedSlayer();
            info.setWeaponType((WeaponType)weapon);
            info.setShieldType((ShieldType)shield);
            const FourBitReading reading = readAsAFourBitClient(slayerOutlookOnTheWire(info));
            EXPECT_EQ(kFourBitWeaponDWORDs[weapon] | kShieldDWORDs[shield], reading.outlook)
                << "weapon " << weapon << ", shield " << shield;
            EXPECT_EQ(seen, reading.weapon) << "weapon " << weapon << ", shield " << shield;
            EXPECT_EQ(shield, reading.shield) << "weapon " << weapon << ", shield " << shield;
        }
    }
    EXPECT_EQ(WEAPON_CROSS, readAsAFourBitClient(0x00007800 | 0x00020000).weapon);
    EXPECT_EQ(WEAPON_NONE, readAsAFourBitClient(0x00040000).weapon);
    EXPECT_EQ(WEAPON_NONE, readAsAFourBitClient(0x00060000).weapon);
}

// New client, old server, or a Slayer.Shape row written without the
// code: bits 17-18 are clear, so the outlook reads back as its four bits
// say: weapons 0..15 as themselves, and the clamped DWORDs of cross1,
// mace and mace1 as a cross, no weapon and no weapon.
TEST(PCSlayerInfoOutlook, anOutlookWithoutTheCodeReadsAsItsFourBits) {
    for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++) {
        const int clamped = weapon < 16 ? weapon : weapon == WEAPON_CROSS1 ? (int)WEAPON_CROSS : (int)WEAPON_NONE;
        for (int shield = SHIELD_NONE; shield < SHIELD_MAX; shield++) {
            PCSlayerInfo stored = namedSlayer();
            Color_t colors[PCSlayerInfo::SLAYER_COLOR_MAX] = {};
            stored.setShapeInfo(kFourBitWeaponDWORDs[weapon] | kShieldDWORDs[shield], colors);
            EXPECT_EQ(clamped, stored.getWeaponType()) << "weapon " << weapon << ", shield " << shield;
            EXPECT_EQ(shield, stored.getShieldType()) << "weapon " << weapon << ", shield " << shield;
        }
    }
}

// Every combination of the two weapon fields decodes to a weapon a table
// of WEAPON_MAX entries can hold: the code when it is set, whatever the
// four bits say, and the four bits otherwise. Each is tried under every
// shield, alone and with every other bit of the DWORD set.
TEST(PCSlayerInfoOutlook, everyOutlookDecodesToAWeaponBelowTheMax) {
    const DWORD kWeaponFields = 0x00007800 | 0x00060000;
    const DWORD kShieldField = 0x00018000;
    for (DWORD low = 0; low < 16; low++) {
        for (DWORD extension = 0; extension < 4; extension++) {
            const int expected = extension != 0 ? (int)(WEAPON_CROSS + extension) : (int)low;
            for (int shield = SHIELD_NONE; shield < SHIELD_MAX; shield++) {
                for (DWORD others : {(DWORD)0, (DWORD) ~(kWeaponFields | kShieldField)}) {
                    const DWORD outlook = (low << 11) | (extension << 17) | kShieldDWORDs[shield] | others;
                    PCSlayerInfo info = namedSlayer();
                    Color_t colors[PCSlayerInfo::SLAYER_COLOR_MAX] = {};
                    info.setShapeInfo(outlook, colors);
                    EXPECT_EQ(expected, info.getWeaponType()) << std::hex << "outlook 0x" << outlook;
                    EXPECT_LT((int)info.getWeaponType(), (int)WEAPON_MAX) << std::hex << "outlook 0x" << outlook;
                    EXPECT_EQ(shield, info.getShieldType()) << std::hex << "outlook 0x" << outlook;
                }
            }
        }
    }
}

// A value past WEAPON_MACE1 is no weapon: both weapon fields cleared,
// nothing spilled into the shield or past the code, whatever was held.
TEST(PCSlayerInfoOutlook, aValuePastMaceOneWritesNoWeapon) {
    for (int past = WEAPON_MAX; past < 32; past++) {
        for (int from = WEAPON_NONE; from < WEAPON_MAX; from++) {
            for (int shield = SHIELD_NONE; shield < SHIELD_MAX; shield++) {
                PCSlayerInfo info = namedSlayer();
                info.setShieldType((ShieldType)shield);
                info.setWeaponType((WeaponType)from);
                info.setWeaponType((WeaponType)past);
                EXPECT_EQ(WEAPON_NONE, info.getWeaponType()) << past << " over " << from << ", shield " << shield;
                EXPECT_EQ(shield, info.getShieldType()) << past << " over " << from << ", shield " << shield;
                EXPECT_EQ(kShieldDWORDs[shield], slayerOutlookOnTheWire(info))
                    << past << " over " << from << ", shield " << shield;
            }
        }
    }
}

// weaponBits, which setWeaponType writes through, against the tables and
// against the DWORD the setter puts on the wire; past the enum, any value
// writes nothing. The gameserver's encoder, which feeds weaponBits the
// shape of the item a slayer holds, is pinned in
// tests/slayer_list_weapon_test.cpp.
TEST(PCSlayerInfoOutlook, weaponBitsMatchTheTablesAndTheSetter) {
    for (DWORD weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++) {
        EXPECT_EQ(kFourBitWeaponDWORDs[weapon] | kExtensionDWORDs[weapon], PCSlayerInfo::weaponBits(weapon)) << weapon;
        PCSlayerInfo info = namedSlayer();
        info.setWeaponType((WeaponType)weapon);
        EXPECT_EQ(PCSlayerInfo::weaponBits(weapon), slayerOutlookOnTheWire(info)) << weapon;
    }
    for (DWORD past : {(DWORD)WEAPON_MAX, (DWORD)31, (DWORD)32, (DWORD)255, (DWORD)0x10011, (DWORD)0xFFFFFFFF})
        EXPECT_EQ(0u, PCSlayerInfo::weaponBits(past)) << past;
    EXPECT_EQ(0x00007800u | 0x00060000u, PCSlayerInfo::kWeaponBitsMask);
}

// How many values an enum with no fixed underlying type and no negative
// enumerator can hold: those of the narrowest bit-field that fits its
// largest enumerator. A value past them is not a value of the type, and
// this suite's Debug build checks every enum load (UBSan) and stops on
// one before any setter could mask it.
constexpr DWORD enumValueCount(DWORD largestEnumerator) {
    return std::bit_ceil(largestEnumerator + 1);
}

// The outlook's other multi-bit fields, as their setters write them: the
// first bit and the mask, spelled as literals so the layout is pinned
// too, and how many values the field's enum type can hold. HelmetType
// runs to HELMET_MAX (4), so it holds 0..7 in a two-bit field; the other
// types fit their fields.
struct OutlookField {
    const char* name;
    int first;
    DWORD mask;
    DWORD values;
    void (*set)(PCSlayerInfo&, DWORD);
    DWORD (*get)(const PCSlayerInfo&);
};
const OutlookField kOutlookFields[] = {
    {"hair style", 1, 3, enumValueCount(HAIR_STYLE3),
     [](PCSlayerInfo& info, DWORD value) { info.setHairStyle((HairStyle)value); },
     [](const PCSlayerInfo& info) { return (DWORD)info.getHairStyle(); }},
    {"helmet", 3, 3, enumValueCount(HELMET_MAX),
     [](PCSlayerInfo& info, DWORD value) { info.setHelmetType((HelmetType)value); },
     [](const PCSlayerInfo& info) { return (DWORD)info.getHelmetType(); }},
    {"jacket", 5, 7, enumValueCount(JACKET_MAX),
     [](PCSlayerInfo& info, DWORD value) { info.setJacketType((JacketType)value); },
     [](const PCSlayerInfo& info) { return (DWORD)info.getJacketType(); }},
    {"pants", 8, 7, enumValueCount(PANTS_MAX),
     [](PCSlayerInfo& info, DWORD value) { info.setPantsType((PantsType)value); },
     [](const PCSlayerInfo& info) { return (DWORD)info.getPantsType(); }},
    {"shield", 15, 3, enumValueCount(SHIELD_MAX),
     [](PCSlayerInfo& info, DWORD value) { info.setShieldType((ShieldType)value); },
     [](const PCSlayerInfo& info) { return (DWORD)info.getShieldType(); }},
};

// Every value a field's type can hold is written into that field alone: a
// value wider than the field keeps only the field's low bits, and no bit
// reaches a neighbour. A helmet of 4..7 would otherwise land on the
// jacket. Every weapon is held while the field is written, so the whole
// DWORD must be that weapon's, with the field's bits and nothing else.
TEST(PCSlayerInfoOutlook, everyValueAFieldsTypeHoldsStaysInTheField) {
    for (const OutlookField& field : kOutlookFields) {
        for (DWORD value = 0; value < field.values; value++) {
            for (int weapon = WEAPON_NONE; weapon < WEAPON_MAX; weapon++) {
                PCSlayerInfo info = namedSlayer();
                info.setWeaponType((WeaponType)weapon);
                field.set(info, field.mask);
                field.set(info, value);
                EXPECT_EQ(weapon, info.getWeaponType()) << field.name << " " << value << ", weapon " << weapon;
                EXPECT_EQ(value & field.mask, field.get(info)) << field.name << " " << value << ", weapon " << weapon;
                EXPECT_EQ(kFourBitWeaponDWORDs[weapon] | kExtensionDWORDs[weapon] |
                              ((value & field.mask) << field.first),
                          slayerOutlookOnTheWire(info))
                    << field.name << " " << value << ", weapon " << weapon;
            }
        }
    }
}

// The character list of a slayer holding a mace: the only fixture whose
// weapon needs the extension code. The canonical LCPCList fixture holds
// a cross, which needs none.
TEST(LCPCListTest, maceSlayerBodyBytesMatchGolden) {
    LCPCList packet;
    PCSlayerInfo* pSlayer = makeSlayerInfo();
    pSlayer->setWeaponType(WEAPON_MACE);
    packet.setPCInfo(SLOT1, pSlayer);
    packet.setPCInfo(SLOT2, makeVampireInfo());
    packet.setPCInfo(SLOT3, makeOustersInfo());
    ASSERT_EQ(WEAPON_MACE, dynamic_cast<const PCSlayerInfo*>(packet.getPCInfo(SLOT1))->getWeaponType());
    ASSERT_EQ(SHIELD2, dynamic_cast<const PCSlayerInfo*>(packet.getPCInfo(SLOT1))->getShieldType());

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("LCPCList.mace", kPlainCode, body);

    LCPCList dst;
    roundTrip(packet, dst, kPlainCode);
    expectEqual(packet, dst);
}

} // namespace
