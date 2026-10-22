//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_session_test.cpp
// Description : Golden byte fixtures, loopback round trips and size
//               pins for the last two client/game-server families
//               without one: taking goods out of a quest stash and
//               wearing them, and the session traffic a client and a
//               game server exchange around a connection - asking for
//               another player's address, asking for a value, logging
//               out, the authentication key, the handover to another
//               server, the port check, the crash report, the lottery
//               pick, the pet gamble and the typed string list.
//
//               The set is taken from the code that sends them: the
//               handlers under src/server/gameserver/handler
//               (CGTakeOutGoodHandler, CGRequestIPHandler,
//               CGRequestInfoHandler, CGLogoutHandler,
//               CGAuthKeyHandler, CGPortCheckHandler,
//               CGCrashReportHandler, CGLotterySelectHandler,
//               CGPetGambleHandler, CGTypeStringListHandler) and the
//               senders outside handler/: quest/ActionTakeOutGoods.cpp,
//               quest/ActionGiveLotto.cpp, Slayer.cpp, Vampire.cpp,
//               Ousters.cpp and EventAuth.cpp. Eighteen packets, each
//               with the reason it is here:
//
//               CGTakeOutGood    the client asks for one of the goods
//                                a quest left in the stash, and
//               GCTakeOutOK      it is told the item came out,
//               GCTakeOutFail    or that it did not.
//               GCGoodsList      the listing of what is waiting, one
//                                record per item, with the options and
//                                the time limit each carries.
//               GCTakeOff        a wear slot is emptied, as the zone
//                                sees it.
//               GCRealWearingInfo    the bitset of the slots the
//                                character is really wearing, which
//                                the three races recompute whenever
//                                gear changes.
//
//               CGRequestIP      the client asks where another player
//                                is connected from, and
//               GCRequestedIP    the answer: the name, the address and
//                                the port.
//               CGRequestInfo    a code and a value, the general
//                                request for a piece of character
//                                information.
//               CGLogout         the client says it is leaving; the
//                                packet has no body at all.
//               CGAuthKey        the client returns the authentication
//                                key, and
//               GCAuthKey        the server hands one out.
//               GCReconnect      the handover: drop this connection
//                                and open one to the address in the
//                                packet, with this key.
//               CGPortCheck      the login server tells a game server
//                                which character is about to connect;
//                                a datagram, so it carries the sender
//                                address in its own header and only
//                                the character name in its body.
//               CGCrashReport    what the client sends after a crash:
//                                a fixed-width build time and fault
//                                address, a version word, and three
//                                WORD-counted strings.
//               CGLotterySelect  the pick, the scratch and the end of
//                                the lottery event.
//               CGPetGamble      the pet gamble request; no body.
//               CGTypeStringList a typed list of strings with one
//                                parameter, which the couple handlers
//                                use to ask for a meeting or a parting.
//
//               Deliberately excluded:
//
//               No packet in this family reads or writes the
//               encrypter. The nineteen src/Core sources that call
//               readEncrypt or writeEncrypt are CGAddMouseToZone,
//               CGAddZoneToInventory, CGAddZoneToMouse, CGAttack,
//               CGDissectionCorpse, CGDropMoney, CGMove,
//               CGNPCAskAnswer, CGPickupMoney, CGSkillToInventory,
//               CGSkillToObject, CGSkillToSelf, CGSkillToTile,
//               CGUseItemFromGear, CGUseItemFromInventory,
//               CGUsePotionFromInventory, GCAddItemToZone,
//               GCMoveError and GCMoveOK, all pinned at encrypt codes
//               0..5 by tests/packet_encrypter_test.cpp. None of the
//               eighteen derives from one of them: seventeen extend
//               Packet directly and CGPortCheck extends
//               DatagramPacket, which calls neither. So each stream
//               golden is recorded at code 0 and its test also asserts
//               the bytes do not vary with the code - adopting the
//               encrypter fails loudly instead of silently voiding the
//               pin.
//
//               Every one of the eighteen has a gameserver row in
//               tests/ratchet/factory_registrations.txt, and none had
//               a golden before this file. CGTypeStringList had an
//               entry-count pin in tests/packet_inventory_test.cpp and
//               CGCrashReport a string-cap pin in
//               tests/packet_roundtrip_test.cpp; both are single-aspect
//               and neither had a golden, so both packets are pinned in
//               full here.
//
//               With this file every registered GC/CG factory has a
//               golden: the names in factory_registrations.txt minus
//               the names under tests/golden/ is empty.
//
//               GCReconnect and GCAuthKey have no live sender -
//               nothing outside src/Core constructs a GCReconnect, and
//               the one GCAuthKey site in EventAuth.cpp is commented
//               out. Both are pinned anyway, because their factories
//               ARE registered: a registered factory is the wire
//               contract the client's own copy has to match, and a GC
//               packet the client can only receive needs no server
//               sender to be part of it.
//
//               Each packet gets three pins (SESSION_PACKET_TESTS):
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
//               CGPortCheck gets the datagram twin of those three
//               (SESSION_DATAGRAM_TESTS): its read() and write() take
//               a Datagram and DatagramPacket refuses a TCP stream by
//               design, so it is pinned through a real Datagram whose
//               header carries a MEASURED body length. That makes the
//               size pin three-way - the size field, the datagram's
//               length and getPacketSize() must name the same count -
//               and there is no encrypt code on that path at all,
//               which is why its golden carries no per-code assertion.
//
//               Extra goldens cover the branches one fixture cannot:
//               the goods listing with nothing in it (.empty) and with
//               the twenty records MAX_GOODS_LIST budgets (.full); the
//               address request with no name (.noname), the branch
//               write() takes when the count byte is zero; the string
//               list with no strings (.empty) and with the twenty of
//               full width its factory max budgets (.full); and the
//               crash report whose three optional strings are all
//               empty (.nostrings), which write() emits as three bare
//               zero-length words and read() takes back.
//
//               Fixture values are distinct per field and >= 128 in
//               every byte the width allows. Four groups cannot follow
//               that rule and say so at the point of use: the names,
//               addresses, build time and call stack, which are text;
//               the request code, the lottery type and the string-list
//               type, which are code bytes with a named range; the
//               character name in CGRequestIP and GCRequestedIP, which
//               stops at the ten their factory maxima budget; and
//               CGCrashReport's build time and fault address, which
//               write() requires to be exactly 19 and 10 bytes.
//
//               What the two halves agree on, each pinned by a test of
//               its own beside the three above. No valid packet's bytes
//               moved for any of them, and docs/reviews/FIXES.md carries the
//               wire consequence of each:
//
//               - GCReconnect declares the body it writes. The pc-type
//                 byte getPacketSize() counted is gone, with the member
//                 nothing wrote, read or initialised and the accessors
//                 on it: the client's own copy reads a name, an address
//                 and a key, and no pc type.
//               - CGRequestIP and GCRequestedIP hold the name to the
//                 ten their factory maxima budget, in the setter, in
//                 write() and in read().
//               - CGRequestIP::read clears the name the packet holds
//                 when the count byte is zero.
//               - GCRequestedIP refuses a nameless answer on both
//                 sides.
//               - GCGoodsList refuses a listing past MAX_GOODS_LIST in
//                 the adder, in write() and in read(), and a record
//                 option count past the 255 GoodsInfo::getPacketMaxSize
//                 budgets; neither count is narrowed to a BYTE before
//                 it is tested.
//               - GCGoodsList::read replaces the listing the packet
//                 holds, freeing the records it owned. The records
//                 belong to the packet: its one sender allocates each
//                 fresh and keeps none, and popGoodsInfo() hands one
//                 back to the caller to free.
//               - GCGoodsList refuses a null record and refuses to pop
//                 from an empty listing.
//               - CGRequestInfo, CGLotterySelect and CGTypeStringList
//                 test the code byte against the range their own
//                 headers name before storing it.
//               - CGCrashReport::read takes the three optional strings
//                 empty, which is the shape write() emits, and write()
//                 refuses a fixed-width field of the wrong width
//                 through InvalidProtocolException rather than an
//                 Assert() that appends to assertion_failed.log in the
//                 working directory before it throws.
//               - All eighteen initialise every member write() emits.
//
//               One finding is recorded here rather than fixed:
//
//               - CGPortCheck is registered on the game server's
//                 client-facing dispatch table although it is a
//                 DatagramPacket, so a client that sends its id over a
//                 TCP socket is refused by
//                 DatagramPacket::read(SocketInputStream&). The
//                 registration cannot be dropped for that: a server
//                 process builds one factory table, and the game
//                 server's UDP path resolves the same one -
//                 Datagram::read(DatagramPacket*&) asks the factory
//                 table on the kernel context for the packet, and
//                 Datagram::isDatagram names PACKET_CG_PORT_CHECK - so
//                 the entry is what lets the packet arrive at all. Its
//                 handler reads the sender address out of the datagram
//                 header, not out of the body pinned here.
//
//////////////////////////////////////////////////////////////////////

#include <cstring>
#include <new>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "CGAuthKey.h"
#include "CGCrashReport.h"
#include "CGLogout.h"
#include "CGLotterySelect.h"
#include "CGPetGamble.h"
#include "CGPortCheck.h"
#include "CGRequestIP.h"
#include "CGRequestInfo.h"
#include "CGTakeOutGood.h"
#include "CGTypeStringList.h"
#include "Datagram.h"
#include "DatagramPacket.h"
#include "Exception.h"
#include "GCAuthKey.h"
#include "GCGoodsList.h"
#include "GCRealWearingInfo.h"
#include "GCReconnect.h"
#include "GCRequestedIP.h"
#include "GCTakeOff.h"
#include "GCTakeOutFail.h"
#include "GCTakeOutOK.h"
#include "TestStreams.h"

using wiretest::expectGolden;
using wiretest::kEncryptCodeCount;
using wiretest::kEncryptCodes;
using wiretest::Loopback;
using wiretest::roundTrip;
using wiretest::writeBody;

namespace {

// The unencrypted branch. No packet in this file rides the encrypter, so
// this is the only code whose bytes could differ from any other.
const uchar kPlainCode = 0;

//////////////////////////////////////////////////////////////////////
// Datagram plumbing, for the one DatagramPacket in the set.
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

// Let a packet parse a body assembled field by field, so a test can put
// a value on the wire that no setter admits.
template <typename Emit> void readHandBuiltBody(Packet& packet, Emit emit) {
    Loopback link;
    link.setCodes(kPlainCode);
    emit(link.out());
    const uint length = link.out().length();
    link.pump(length);
    packet.read(link.in());
}

//////////////////////////////////////////////////////////////////////
// The three pins every packet gets. fill() / expectEqual() are
// overloaded per packet, so the same canonical instance feeds all three.
// Both take a non-const reference: several getters are not const, and
// draining a goods listing goes through a destructive accessor.
//////////////////////////////////////////////////////////////////////

#define SESSION_PACKET_TESTS(Name)                                                                   \
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
                << #Name " now varies with the encrypt code - add per-code goldens";                 \
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

// A second fixture for a packet whose write() has a branch the canonical
// one does not take.
#define SESSION_PACKET_VARIANT(Name, Variant, fillVariant)                     \
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

// The datagram twin. The round trip runs through a real Datagram rather
// than a socket stream, and there is no encrypt code to vary.
#define SESSION_DATAGRAM_TESTS(Name)                                                                 \
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
        EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());                               \
        EXPECT_EQ(factory.getPacketID(), packet.getPacketID());                                      \
        EXPECT_EQ(factory.getPacketName(), packet.getPacketName());                                  \
    }

//////////////////////////////////////////////////////////////////////
// Taking goods out of the quest stash and wearing them.
//////////////////////////////////////////////////////////////////////

void fill(CGTakeOutGood& packet) {
    packet.setObjectID(0x81A2B3C4);
}

void expectEqual(CGTakeOutGood& a, CGTakeOutGood& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

SESSION_PACKET_TESTS(CGTakeOutGood)

void fill(GCTakeOutOK& packet) {
    packet.setObjectID(0x82A3B4C5);
}

void expectEqual(GCTakeOutOK& a, GCTakeOutOK& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

SESSION_PACKET_TESTS(GCTakeOutOK)

void fill(GCTakeOutFail& packet) {
    packet.setObjectID(0x83A4B5C6);
}

void expectEqual(GCTakeOutFail& a, GCTakeOutFail& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
}

SESSION_PACKET_TESTS(GCTakeOutFail)

// SlotID_t is a BYTE typedef rather than an enum, so the wear slot
// carries a high byte like every other full-width field.
void fill(GCTakeOff& packet) {
    packet.setObjectID(0x84A5B6C7);
    packet.setSlotID(0x88);
}

void expectEqual(GCTakeOff& a, GCTakeOff& b) {
    EXPECT_EQ(a.getObjectID(), b.getObjectID());
    EXPECT_EQ(a.getSlotID(), b.getSlotID());
}

SESSION_PACKET_TESTS(GCTakeOff)

void fill(GCRealWearingInfo& packet) {
    packet.setInfo(0x89AABBCC);
}

void expectEqual(GCRealWearingInfo& a, GCRealWearingInfo& b) {
    EXPECT_EQ(a.getInfo(), b.getInfo());
}

SESSION_PACKET_TESTS(GCRealWearingInfo)

// One record of the goods listing. Grade_t is a signed int, so its
// fixture is a high-byte pattern converted to it explicitly.
GoodsInfo* makeGoods(ObjectID_t objectID, BYTE itemClass, ItemType_t itemType, unsigned int grade, uint optionCount,
                     ItemNum_t num, DWORD timeLimit) {
    GoodsInfo* pGI = new GoodsInfo;
    pGI->objectID = objectID;
    pGI->itemClass = itemClass;
    pGI->itemType = itemType;
    pGI->grade = (Grade_t)grade;
    pGI->num = num;
    pGI->timeLimit = timeLimit;
    for (uint i = 0; i < optionCount; i++)
        pGI->optionType.push_back((OptionType_t)(0x91 + i));
    return pGI;
}

void expectGoodsEqual(const GoodsInfo& a, const GoodsInfo& b) {
    EXPECT_EQ(a.objectID, b.objectID);
    EXPECT_EQ((int)a.itemClass, (int)b.itemClass);
    EXPECT_EQ(a.itemType, b.itemType);
    EXPECT_EQ(a.grade, b.grade);
    EXPECT_EQ((int)a.num, (int)b.num);
    EXPECT_EQ(a.timeLimit, b.timeLimit);
    ASSERT_EQ(a.optionType.size(), b.optionType.size());
    list<OptionType_t>::const_iterator x = a.optionType.begin();
    list<OptionType_t>::const_iterator y = b.optionType.begin();
    for (; x != a.optionType.end(); ++x, ++y)
        EXPECT_EQ((int)*x, (int)*y);
}

// Three records: one with options, one with none, one with a single
// option, so the inner loop is entered, skipped and entered again.
void fill(GCGoodsList& packet) {
    packet.addGoodsInfo(makeGoods(0x8CADBECF, 0x90, 0x91A2, 0x93A4B5C6, 3, 0x97, 0x98A9BACB));
    packet.addGoodsInfo(makeGoods(0x9CADBECF, 0xA0, 0xA1B2, 0xA3B4C5D6, 0, 0xA7, 0xA8B9CADB));
    packet.addGoodsInfo(makeGoods(0xACBDCEDF, 0xB0, 0xB1C2, 0xB3C4D5E6, 1, 0xB7, 0xB8C9DAEB));
}

// The listing is readable only through the destructive popGoodsInfo, so
// comparing two packets drains both. getPacketSize() shrinks with the
// list, which is what bounds the walk: the packet exposes no count.
void expectEqual(GCGoodsList& a, GCGoodsList& b) {
    ASSERT_EQ(a.getPacketSize(), b.getPacketSize());
    int record = 0;
    while (a.getPacketSize() > (PacketSize_t)szBYTE) {
        GoodsInfo* x = a.popGoodsInfo();
        GoodsInfo* y = b.popGoodsInfo();
        ASSERT_TRUE(x != NULL);
        ASSERT_TRUE(y != NULL);
        SCOPED_TRACE(record++);
        expectGoodsEqual(*x, *y);
        delete x;
        delete y;
    }
    EXPECT_EQ(a.getPacketSize(), b.getPacketSize());
}

SESSION_PACKET_TESTS(GCGoodsList)

// The count byte alone, with no record behind it.
void fillNoGoods(GCGoodsList&) {}

SESSION_PACKET_VARIANT(GCGoodsList, empty, fillNoGoods)

// The twenty records MAX_GOODS_LIST admits, the most write() lets
// through.
void fillFullGoods(GCGoodsList& packet) {
    for (uint i = 0; i < (uint)MAX_GOODS_LIST; i++)
        packet.addGoodsInfo(makeGoods((ObjectID_t)(0x81A2B300 + i), (BYTE)(0x90 + i), (ItemType_t)(0x91A2 + i),
                                      0x93A4B5C6 + i, i % 4, (ItemNum_t)(0x97 + i), 0x98A9BACB + i));
}

SESSION_PACKET_VARIANT(GCGoodsList, full, fillFullGoods)

//////////////////////////////////////////////////////////////////////
// Asking for an address, a value, and saying goodbye.
//////////////////////////////////////////////////////////////////////

// The name is text, and the factory max budgets ten bytes for it, so the
// fixture is exactly that wide rather than a high-byte string.
void fill(CGRequestIP& packet) {
    packet.setName("IPTargetXY");
}

void expectEqual(CGRequestIP& a, CGRequestIP& b) {
    EXPECT_EQ(a.getName(), b.getName());
}

SESSION_PACKET_TESTS(CGRequestIP)

// The other branch of write(): a zero count byte and nothing behind it.
void fillNoName(CGRequestIP& packet) {
    packet.setName("");
}

SESSION_PACKET_VARIANT(CGRequestIP, noname, fillNoName)

// The name is text held to the ten the factory max budgets; the address
// and the port are full-width.
void fill(GCRequestedIP& packet) {
    packet.setName("IPOwnerXYZ");
    packet.setIP(0x81A2B3C4);
    packet.setPort(0x85A6B7C8);
}

void expectEqual(GCRequestedIP& a, GCRequestedIP& b) {
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getIP(), b.getIP());
    EXPECT_EQ(a.getPort(), b.getPort());
}

SESSION_PACKET_TESTS(GCRequestedIP)

// The code byte names one request, so it carries that enumerator rather
// than a high byte; the value is full-width.
void fill(CGRequestInfo& packet) {
    packet.setCode(CGRequestInfo::REQUEST_CHARACTER_INFO);
    packet.setValue(0x86A7B8C9);
}

void expectEqual(CGRequestInfo& a, CGRequestInfo& b) {
    EXPECT_EQ((int)a.getCode(), (int)b.getCode());
    EXPECT_EQ(a.getValue(), b.getValue());
}

SESSION_PACKET_TESTS(CGRequestInfo)

// No body at all: the packet id alone says the client is leaving.
void fill(CGLogout&) {}

void expectEqual(CGLogout&, CGLogout&) {}

SESSION_PACKET_TESTS(CGLogout)

//////////////////////////////////////////////////////////////////////
// The authentication key and the handover to another server.
//////////////////////////////////////////////////////////////////////

void fill(CGAuthKey& packet) {
    packet.setKey(0x87A8B9CA);
}

void expectEqual(CGAuthKey& a, CGAuthKey& b) {
    EXPECT_EQ(a.getKey(), b.getKey());
}

SESSION_PACKET_TESTS(CGAuthKey)

void fill(GCAuthKey& packet) {
    packet.setKey(0x88A9BACB);
}

void expectEqual(GCAuthKey& a, GCAuthKey& b) {
    EXPECT_EQ(a.getKey(), b.getKey());
}

SESSION_PACKET_TESTS(GCAuthKey)

// The name and the address are text; the address is a full-width dotted
// quad rather than a high-byte string, because write() caps it at 15.
void fill(GCReconnect& packet) {
    packet.setName("ReconnectPC");
    packet.setServerIP("203.198.171.94");
    packet.setKey(0x89AABBCC);
}

void expectEqual(GCReconnect& a, GCReconnect& b) {
    EXPECT_EQ(a.getName(), b.getName());
    EXPECT_EQ(a.getServerIP(), b.getServerIP());
    EXPECT_EQ(a.getKey(), b.getKey());
}

SESSION_PACKET_TESTS(GCReconnect)

//////////////////////////////////////////////////////////////////////
// The port check, the crash report, the lottery and the gamble.
//////////////////////////////////////////////////////////////////////

// The character name is text, held to the twenty read() and write() both
// admit.
void fill(CGPortCheck& packet) {
    packet.setPCName("PortCheckPC");
}

void expectEqual(CGPortCheck& a, CGPortCheck& b) {
    EXPECT_EQ(a.getPCName(), b.getPCName());
}

SESSION_DATAGRAM_TESTS(CGPortCheck)

// write() requires the build time to be exactly 19 bytes and the fault
// address exactly 10, so both are text of that width; the three trailing
// strings are text too. Only the version word can carry high bytes.
const char* const kCrashTime = "2026-01-02 03:04:05";
const char* const kCrashAddress = "0x00401000";

void fill(CGCrashReport& packet) {
    packet.setExecutableTime(kCrashTime);
    packet.setVersion(0x8CAD);
    packet.setAddress(kCrashAddress);
    packet.setOS("Windows-10-x64");
    packet.setCallStack("0x00401000 0x00402000 0x00403000");
    packet.setMessage("unhandled exception in the render loop");
}

void expectEqual(CGCrashReport& a, CGCrashReport& b) {
    EXPECT_EQ(a.getExecutableTime(), b.getExecutableTime());
    EXPECT_EQ(a.getVersion(), b.getVersion());
    EXPECT_EQ(a.getAddress(), b.getAddress());
    EXPECT_EQ(a.getOS(), b.getOS());
    EXPECT_EQ(a.getCallStack(), b.getCallStack());
    EXPECT_EQ(a.getMessage(), b.getMessage());
}

SESSION_PACKET_TESTS(CGCrashReport)

// The type byte names three states, so it carries one of them rather
// than a high byte; the two DWORDs are full-width.
void fill(CGLotterySelect& packet) {
    packet.setType(TYPE_FINISH_SCRATCH);
    packet.setQuestLevel(0x8DAEBFC0);
    packet.setGiftID(0x91A2B3C4);
}

void expectEqual(CGLotterySelect& a, CGLotterySelect& b) {
    EXPECT_EQ((int)a.getType(), (int)b.getType());
    EXPECT_EQ(a.getQuestLevel(), b.getQuestLevel());
    EXPECT_EQ(a.getGiftID(), b.getGiftID());
}

SESSION_PACKET_TESTS(CGLotterySelect)

// No body at all.
void fill(CGPetGamble&) {}

void expectEqual(CGPetGamble&, CGPetGamble&) {}

SESSION_PACKET_TESTS(CGPetGamble)

// The type byte names three kinds of list, so it carries one of them;
// the strings are text and the parameter is full-width.
void fill(CGTypeStringList& packet) {
    packet.setType(CGTypeStringList::STRING_TYPE_WAIT_FOR_APART);
    packet.addString("alpha-string");
    packet.addString("bravo-string");
    packet.addString("charlie-string");
    packet.setParam(0x95A6B7C8);
}

// The listing is readable only through the destructive popString, and
// getSize() bounds the walk.
void expectEqual(CGTypeStringList& a, CGTypeStringList& b) {
    EXPECT_EQ((int)a.getType(), (int)b.getType());
    EXPECT_EQ(a.getParam(), b.getParam());
    ASSERT_EQ(a.getSize(), b.getSize());
    const int held = a.getSize();
    for (int i = 0; i < held; i++)
        EXPECT_EQ(a.popString(), b.popString()) << "list string " << i;
}

SESSION_PACKET_TESTS(CGTypeStringList)

// The count byte alone, with no string behind it.
void fillNoStrings(CGTypeStringList& packet) {
    packet.setType(CGTypeStringList::STRING_TYPE_FORCE_APART_COUPLE);
    packet.setParam(0x96A7B8C9);
}

SESSION_PACKET_VARIANT(CGTypeStringList, empty, fillNoStrings)

// The twenty strings of fifty bytes the factory max budgets, which is
// what both the adder and read() stop at.
void fillFullStrings(CGTypeStringList& packet) {
    packet.setType(CGTypeStringList::STRING_TYPE_WAIT_FOR_MEET);
    for (uint i = 0; i < CGTypeStringList::kMaxStringCount; i++)
        packet.addString(std::string(MAX_STRING_LENGTH, (char)('a' + (i % 26))));
    packet.setParam(0x97A8B9CA);
}

SESSION_PACKET_VARIANT(CGTypeStringList, full, fillFullStrings)

//////////////////////////////////////////////////////////////////////
// What the two halves agree on, each pinned beside the three above.
//////////////////////////////////////////////////////////////////////

// The name is held to the ten the factory max budgets: the setter cuts
// there, and write() and read() carry the field at that width.
TEST(CGRequestIPTest, theNameIsHeldToWhatTheFactoryMaxBudgets) {
    CGRequestIP packet;
    packet.setName("ElevenChars");
    EXPECT_EQ("ElevenChar", packet.getName());

    CGRequestIPFactory factory;
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());

    CGRequestIP dst;
    roundTrip(packet, dst, kPlainCode);
    EXPECT_EQ("ElevenChar", dst.getName());

    CGRequestIP tooLong;
    EXPECT_THROW(readHandBuiltBody(tooLong,
                                   [](SocketEncryptOutputStream& out) {
                                       out.write((BYTE)(CGRequestIP::kMaxNameLength + 1));
                                       out.write("ElevenChars", CGRequestIP::kMaxNameLength + 1);
                                   }),
                 InvalidProtocolException);
}

// CGRequestIP::read clears the name the packet already holds when the
// count byte is zero, so a packet read into twice carries what the
// second body said.
TEST(CGRequestIPTest, aSecondReadWithNoNameClearsTheNameItHolds) {
    CGRequestIP named;
    fill(named);

    CGRequestIP dst;
    roundTrip(named, dst, kPlainCode);
    ASSERT_EQ("IPTargetXY", dst.getName());

    CGRequestIP nameless;
    fillNoName(nameless);
    roundTrip(nameless, dst, kPlainCode);
    EXPECT_EQ("", dst.getName());
}

// GCRequestedIP's name is held the same way.
TEST(GCRequestedIPTest, theNameIsHeldToWhatTheFactoryMaxBudgets) {
    GCRequestedIP packet;
    packet.setName("ElevenChars");
    packet.setIP(0x81A2B3C4);
    packet.setPort(0x85A6B7C8);
    EXPECT_EQ("ElevenChar", packet.getName());

    GCRequestedIPFactory factory;
    EXPECT_LE(packet.getPacketSize(), factory.getPacketMaxSize());

    GCRequestedIP dst;
    roundTrip(packet, dst, kPlainCode);
    EXPECT_EQ("ElevenChar", dst.getName());

    GCRequestedIP tooLong;
    EXPECT_THROW(readHandBuiltBody(tooLong,
                                   [](SocketEncryptOutputStream& out) {
                                       out.write((BYTE)(GCRequestedIP::kMaxNameLength + 1));
                                       out.write("ElevenChars", GCRequestedIP::kMaxNameLength + 1);
                                       out.write((IP_t)0x81A2B3C4);
                                       out.write((uint)0x85A6B7C8);
                                   }),
                 InvalidProtocolException);
}

// Both halves refuse a nameless answer, so the shape write() cannot emit
// is the shape read() cannot accept.
TEST(GCRequestedIPTest, aNamelessAnswerIsRefusedOnBothSides) {
    GCRequestedIP packet;
    packet.setIP(0x81A2B3C4);
    packet.setPort(0x85A6B7C8);
    EXPECT_THROW(writeBody(packet, kPlainCode), InvalidProtocolException);

    GCRequestedIP dst;
    EXPECT_THROW(readHandBuiltBody(dst,
                                   [](SocketEncryptOutputStream& out) {
                                       out.write((BYTE)0);
                                       out.write((IP_t)0x81A2B3C4);
                                       out.write((uint)0x85A6B7C8);
                                   }),
                 InvalidProtocolException);
}

// The listing stops at MAX_GOODS_LIST on every side. The adder refuses
// the record past it, so write()'s own test - which runs before the
// count is narrowed to the BYTE that describes it - has nothing left to
// catch.
TEST(GCGoodsListTest, aListingPastTheMaximumIsRefused) {
    GCGoodsList packet;
    fillFullGoods(packet);

    GoodsInfo* pOneTooMany = makeGoods(0x81A2B3C4, 0x90, 0x91A2, 0x93A4B5C6, 0, 0x97, 0x98A9BACB);
    EXPECT_THROW(packet.addGoodsInfo(pOneTooMany), InvalidProtocolException);
    delete pOneTooMany;

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    ASSERT_FALSE(body.empty());
    EXPECT_EQ(MAX_GOODS_LIST, (int)body[0]);
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());

    GCGoodsList dst;
    EXPECT_THROW(readHandBuiltBody(dst, [](SocketEncryptOutputStream& out) { out.write((BYTE)(MAX_GOODS_LIST + 1)); }),
                 DisconnectException);
}

// A record's option count stops at the 255 GoodsInfo::getPacketMaxSize
// budgets, which is also every value the count byte can describe.
TEST(GCGoodsListTest, aRecordOptionCountPastTheBudgetIsRefused) {
    GCGoodsList packet;

    GoodsInfo* pOneTooMany =
        makeGoods(0x81A2B3C4, 0x90, 0x91A2, 0x93A4B5C6, GCGoodsList::kMaxOptionCount + 1, 0x97, 0x98A9BACB);
    EXPECT_THROW(packet.addGoodsInfo(pOneTooMany), InvalidProtocolException);
    delete pOneTooMany;

    packet.addGoodsInfo(
        makeGoods(0x81A2B3C4, 0x90, 0x91A2, 0x93A4B5C6, GCGoodsList::kMaxOptionCount, 0x97, 0x98A9BACB));

    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    // The count byte sits behind the record's object id, class, type and
    // grade.
    const size_t countAt = szBYTE + szObjectID + szBYTE + szItemType + szGrade;
    ASSERT_LT(countAt, body.size());
    EXPECT_EQ((int)GCGoodsList::kMaxOptionCount, (int)body[countAt]);
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());
}

// GCGoodsList::read replaces the listing the packet already holds, so a
// packet read into twice declares and writes the second listing alone.
TEST(GCGoodsListTest, aSecondReadReplacesTheListingItAlreadyHolds) {
    GCGoodsList src;
    fill(src);

    GCGoodsList dst;
    roundTrip(src, dst, kPlainCode);
    const PacketSize_t afterOne = dst.getPacketSize();

    GCGoodsList again;
    fill(again);
    roundTrip(again, dst, kPlainCode);

    EXPECT_EQ(afterOne, dst.getPacketSize());
}

// The packet owns its records, so neither end of the listing admits a
// pointer it cannot follow.
TEST(GCGoodsListTest, aNullRecordAndAnEmptyListingAreRefused) {
    GCGoodsList packet;
    EXPECT_THROW(packet.addGoodsInfo(NULL), InvalidProtocolException);
    EXPECT_THROW(packet.popGoodsInfo(), InvalidProtocolException);
}

// CGRequestInfo::read tests the code byte against REQUEST_INFO_MAX, the
// one value its own header names.
TEST(CGRequestInfoTest, aCodeOutsideTheNamedRangeIsRefused) {
    CGRequestInfo packet;
    EXPECT_THROW(readHandBuiltBody(packet,
                                   [](SocketEncryptOutputStream& out) {
                                       out.write((BYTE)CGRequestInfo::REQUEST_INFO_MAX);
                                       out.write((uint)0x86A7B8C9);
                                   }),
                 InvalidProtocolException);
}

// CGLotterySelect::read tests the type byte against TYPE_MAX, the three
// its own header names.
TEST(CGLotterySelectTest, aTypeOutsideTheNamedRangeIsRefused) {
    CGLotterySelect packet;
    EXPECT_THROW(readHandBuiltBody(packet,
                                   [](SocketEncryptOutputStream& out) {
                                       out.write((BYTE)TYPE_MAX);
                                       out.write((DWORD)0x8DAEBFC0);
                                       out.write((DWORD)0x91A2B3C4);
                                   }),
                 InvalidProtocolException);
}

// CGTypeStringList::read tests the type byte against the three
// StringType values its own header names.
TEST(CGTypeStringListTest, aTypeOutsideTheNamedRangeIsRefused) {
    CGTypeStringList packet;
    EXPECT_THROW(readHandBuiltBody(packet,
                                   [](SocketEncryptOutputStream& out) {
                                       out.write((BYTE)(CGTypeStringList::STRING_TYPE_FORCE_APART_COUPLE + 1));
                                       out.write((BYTE)0);
                                       out.write((DWORD)0x95A6B7C8);
                                   }),
                 InvalidProtocolException);
}

// The three optional strings write() admits empty are the three read()
// takes back, so the body write() produces can be read back.
void fillNoOptionalStrings(CGCrashReport& packet) {
    packet.setExecutableTime(kCrashTime);
    packet.setVersion(0x8CAD);
    packet.setAddress(kCrashAddress);
}

TEST(CGCrashReportTest, nostringsBodyBytesMatchGoldenAndReadBack) {
    CGCrashReport packet;
    fillNoOptionalStrings(packet);
    const std::vector<unsigned char> body = writeBody(packet, kPlainCode);
    expectGolden("CGCrashReport.nostrings", kPlainCode, body);
    EXPECT_EQ((size_t)packet.getPacketSize(), body.size());

    CGCrashReport dst;
    roundTrip(packet, dst, kPlainCode);
    expectEqual(packet, dst);
}

// The two fields that travel without a length prefix are refused at any
// other width, through the exception the rest of the packet uses rather
// than an Assert() that writes a log file first.
TEST(CGCrashReportTest, aFixedWidthFieldOfTheWrongWidthIsRefused) {
    CGCrashReport shortTime;
    fill(shortTime);
    shortTime.setExecutableTime("2026-01-02");
    EXPECT_THROW(writeBody(shortTime, kPlainCode), InvalidProtocolException);

    CGCrashReport longAddress;
    fill(longAddress);
    longAddress.setAddress("0x004010000");
    EXPECT_THROW(writeBody(longAddress, kPlainCode), InvalidProtocolException);
}

// Both halves refuse a nameless port check, so the shape write() cannot
// emit is the shape read() cannot accept, and both stop at twenty.
TEST(CGPortCheckTest, theNameIsRefusedEmptyAndPastTwentyOnBothSides) {
    CGPortCheck nameless;
    EXPECT_THROW(datagramBody(nameless), InvalidProtocolException);

    CGPortCheck tooLong;
    tooLong.setPCName(std::string(21, 'n'));
    EXPECT_THROW(datagramBody(tooLong), InvalidProtocolException);

    CGPortCheck atTheCap;
    atTheCap.setPCName(std::string(20, 'n'));
    CGPortCheckFactory factory;
    EXPECT_EQ(factory.getPacketMaxSize(), atTheCap.getPacketSize());

    std::vector<unsigned char> oversized;
    oversized.push_back(21);
    for (int i = 0; i < 21; i++)
        oversized.push_back('n');
    CGPortCheck dst;
    EXPECT_THROW(readDatagramImage(dst, oversized), InvalidProtocolException);
}

//////////////////////////////////////////////////////////////////////
// What the default constructor leaves.
//////////////////////////////////////////////////////////////////////

// Each packet is built twice over storage poisoned with a different
// byte, so a member the constructor leaves alone reaches the wire as
// that byte and the two bodies differ. `prep` sets only what write()
// refuses to run without; it touches no plain scalar.

std::vector<unsigned char> emitBody(const Packet& packet) {
    return writeBody(packet, kPlainCode);
}

std::vector<unsigned char> emitBody(const DatagramPacket& packet) {
    return datagramBody(packet);
}

template <typename PacketType, typename Prep>
std::vector<unsigned char> bodyOverPoison(unsigned char poison, Prep prep) {
    alignas(PacketType) unsigned char storage[sizeof(PacketType)];
    memset(storage, poison, sizeof(storage));
    PacketType* pPacket = new (storage) PacketType();
    prep(*pPacket);
    std::vector<unsigned char> body = emitBody(*pPacket);
    pPacket->~PacketType();
    return body;
}

template <typename PacketType, typename Prep> void expectEveryMemberIsInitialised(const char* what, Prep prep) {
    EXPECT_EQ(bodyOverPoison<PacketType>(0x00, prep), bodyOverPoison<PacketType>(0xFF, prep))
        << what << ": its default constructor leaves a member write() emits uninitialised";
}

template <typename PacketType> void expectEveryMemberIsInitialised(const char* what) {
    expectEveryMemberIsInitialised<PacketType>(what, [](PacketType&) {});
}

TEST(SessionConstructorTest, everyPacketInitialisesEveryMemberItWrites) {
    expectEveryMemberIsInitialised<CGTakeOutGood>("CGTakeOutGood");
    expectEveryMemberIsInitialised<GCTakeOutOK>("GCTakeOutOK");
    expectEveryMemberIsInitialised<GCTakeOutFail>("GCTakeOutFail");
    expectEveryMemberIsInitialised<GCGoodsList>("GCGoodsList");
    expectEveryMemberIsInitialised<GCTakeOff>("GCTakeOff");
    expectEveryMemberIsInitialised<GCRealWearingInfo>("GCRealWearingInfo");
    expectEveryMemberIsInitialised<CGRequestIP>("CGRequestIP");
    // write() refuses an empty name, so the fixture names the character.
    expectEveryMemberIsInitialised<GCRequestedIP>("GCRequestedIP", [](GCRequestedIP& p) { p.setName("IPOwnerXYZ"); });
    expectEveryMemberIsInitialised<CGRequestInfo>("CGRequestInfo");
    expectEveryMemberIsInitialised<CGLogout>("CGLogout");
    expectEveryMemberIsInitialised<CGAuthKey>("CGAuthKey");
    expectEveryMemberIsInitialised<GCAuthKey>("GCAuthKey");
    // write() refuses an empty name and an empty address.
    expectEveryMemberIsInitialised<GCReconnect>("GCReconnect", [](GCReconnect& p) {
        p.setName("ReconnectPC");
        p.setServerIP("203.198.171.94");
    });
    // write() refuses an empty name, and the packet has nothing else to
    // carry.
    expectEveryMemberIsInitialised<CGPortCheck>("CGPortCheck", [](CGPortCheck& p) { p.setPCName("PortCheckPC"); });
    // write() refuses the two fixed-width fields at any other width, so
    // the fixture sets both.
    expectEveryMemberIsInitialised<CGCrashReport>("CGCrashReport", [](CGCrashReport& p) {
        p.setExecutableTime(kCrashTime);
        p.setAddress(kCrashAddress);
    });
    expectEveryMemberIsInitialised<CGLotterySelect>("CGLotterySelect");
    expectEveryMemberIsInitialised<CGPetGamble>("CGPetGamble");
    expectEveryMemberIsInitialised<CGTypeStringList>("CGTypeStringList");
}

} // namespace
