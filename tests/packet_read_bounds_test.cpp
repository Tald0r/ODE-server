//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_read_bounds_test.cpp
// Description : Malformed bodies that reach memory they must not, most
//               of them found by the packet-read fuzz targets
//               (tests/fuzz/), each pinned here as the refusal or the
//               value the read now gives. The inputs a fuzz target found
//               are replayed as well, from tests/fuzz/regressions/, by the
//               fuzz_replay tests.
//
//               A CG or CL case is a body a client can put on the wire,
//               and every packet a server reads is printed with
//               toString(). Most GC cases are bodies only a server
//               sends: the gameserver's client link refuses them before
//               the read, and fuzz_replay_game_any_id reads them with
//               that gate open. Two cases are neither. StoreInfoTest's
//               GCMyStoreInfo is a body the live client does send, but
//               the gameserver refuses it unread and its factory packet
//               holds no record, so no replay reaches StoreInfo::read and
//               this test is the bound's only guard. The PCVampireInfo
//               case is read only inside LCPCList, which the loginserver
//               sends and no server reads.
//
//////////////////////////////////////////////////////////////////////

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "CGConnect.h"
#include "CLCreatePC.h"
#include "CLSelectPC.h"
#include "Exception.h"
#include "GCMyStoreInfo.h"
#include "GCNPCAskVariable.h"
#include "GCShopList.h"
#include "GCShopListMysterious.h"
#include "GCStashList.h"
#include "PCVampireInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketInputStreamTestAccess.h"
#include "StoreInfo.h"
#include "TestStreams.h"

using wiretest::Loopback;

namespace {

const uchar kPlainCode = 0;

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

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

} // namespace

//////////////////////////////////////////////////////////////////////
// A debug string names an enumerator through a table of names, and the
// value came off the wire: one past the table prints as its number
// instead of reading a string object that is not there.
//////////////////////////////////////////////////////////////////////

TEST(DebugNameTest, aCharacterTypePastTheTablePrintsAsItsNumber) {
    CGConnect connect;
    connect.setPCType(PCType(3));
    EXPECT_TRUE(contains(connect.toString(), "PCType:3")) << connect.toString();

    CLSelectPC select;
    select.setPCType(PCType(3));
    EXPECT_TRUE(contains(select.toString(), "PCType:3")) << select.toString();
}

TEST(DebugNameTest, aSlotSexAndHairStylePastTheirTablesPrintAsNumbers) {
    CLCreatePC create;
    create.setSlot(Slot(3));
    create.setSex(Sex(1));
    create.setHairStyle(HairStyle(3));
    const std::string text = create.toString();
    EXPECT_TRUE(contains(text, "Slot:3")) << text;
    EXPECT_TRUE(contains(text, "Sex:MALE")) << text;
    EXPECT_TRUE(contains(text, "HairStyle:3")) << text;
}

// Every value Sex can hold has a name; the slot enumeration has one
// more value than its table. PCVampireInfo is read only inside LCPCList,
// which no client status admits, so this one is not a client's body.
TEST(DebugNameTest, aVampireSlotPastItsTablePrintsAsItsNumber) {
    PCVampireInfo info{};
    info.setSlot(Slot(3));
    info.setSex(Sex(1));
    const std::string text = info.toString();
    EXPECT_TRUE(contains(text, "Slot:3")) << text;
    EXPECT_TRUE(contains(text, "Sex:MALE")) << text;
}

//////////////////////////////////////////////////////////////////////
// A rack or stash listing addresses its slots by an index read off the
// wire. One past the slots is refused before anything is written to it.
//////////////////////////////////////////////////////////////////////

namespace {

// One shop or stash slot's fields after its index, as the readers take
// them, with no options.
void emitShopItem(SocketOutputStream& out) {
    out.write((ObjectID_t)0x81A2B3C4);
    out.write((BYTE)1);
    out.write((ItemType_t)2);
    out.write((BYTE)0); // option count
    out.write((Durability_t)3);
    out.write((Silver_t)4);
    out.write((Grade_t)5);
    out.write((EnchantLevel_t)6);
}

// Both coordinates of a stash slot come off the wire.
void emitStashSlot(SocketOutputStream& out, BYTE rack, BYTE index) {
    out.write((BYTE)1); // stash count
    out.write((BYTE)1); // one item
    out.write(rack);
    out.write(index);
    emitShopItem(out);
    out.write((BYTE)0); // sub-item count
    out.write((Gold_t)0);
}

} // namespace

TEST(RackSlotTest, aShopListSlotPastTheRackIsRefused) {
    auto packet = std::make_unique<GCShopList>();
    EXPECT_THROW(readHandBuiltBody(*packet,
                                   [](SocketOutputStream& out) {
                                       out.write((ObjectID_t)0x81A2B3C4);
                                       out.write((ShopVersion_t)1);
                                       out.write((ShopRackType_t)0);
                                       out.write((BYTE)1); // one item
                                       out.write((BYTE)SHOP_RACK_INDEX_MAX);
                                       emitShopItem(out);
                                       out.write((MarketCond_t)0);
                                       out.write((MarketCond_t)0);
                                       out.write((BYTE)0);
                                   }),
                 InvalidProtocolException);
}

TEST(RackSlotTest, theLastShopListSlotIsRead) {
    auto packet = std::make_unique<GCShopList>();
    readHandBuiltBody(*packet, [](SocketOutputStream& out) {
        out.write((ObjectID_t)0x81A2B3C4);
        out.write((ShopVersion_t)1);
        out.write((ShopRackType_t)0);
        out.write((BYTE)1);
        out.write((BYTE)(SHOP_RACK_INDEX_MAX - 1));
        emitShopItem(out);
        out.write((MarketCond_t)0);
        out.write((MarketCond_t)0);
        out.write((BYTE)0);
    });
    EXPECT_TRUE(packet->getShopItem(SHOP_RACK_INDEX_MAX - 1).bExist);
}

TEST(RackSlotTest, aMysteriousShopListSlotPastTheRackIsRefused) {
    auto packet = std::make_unique<GCShopListMysterious>();
    EXPECT_THROW(readHandBuiltBody(*packet,
                                   [](SocketOutputStream& out) {
                                       out.write((ObjectID_t)0x81A2B3C4);
                                       out.write((ShopVersion_t)1);
                                       out.write((ShopRackType_t)0);
                                       out.write((BYTE)1);
                                       out.write((BYTE)SHOP_RACK_INDEX_MAX);
                                       out.write((BYTE)1);
                                       out.write((ItemType_t)2);
                                       out.write((MarketCond_t)0);
                                       out.write((MarketCond_t)0);
                                   }),
                 InvalidProtocolException);
}

TEST(RackSlotTest, aStashSlotPastTheRackOrTheRowIsRefused) {
    auto pastRow = std::make_unique<GCStashList>();
    EXPECT_THROW(readHandBuiltBody(*pastRow, [](SocketOutputStream& out) { emitStashSlot(out, 0, STASH_INDEX_MAX); }),
                 InvalidProtocolException);

    auto pastRack = std::make_unique<GCStashList>();
    EXPECT_THROW(readHandBuiltBody(*pastRack, [](SocketOutputStream& out) { emitStashSlot(out, STASH_RACK_MAX, 0); }),
                 InvalidProtocolException);
}

//////////////////////////////////////////////////////////////////////
// A bool on the wire is one byte, and a sender can put any byte there.
//////////////////////////////////////////////////////////////////////

TEST(WireBoolTest, everyNonzeroByteReadsAsTrueAndIsStoredAsOne) {
    const unsigned char bytes[] = {0x00, 0x01, 0x02, 0xFF};
    Socket socket(new SocketImpl());
    SocketInputStream in(&socket, 16);
    ASSERT_TRUE(SocketInputStreamTestAccess::Preload(in, bytes, sizeof(bytes)));

    for (unsigned char sent : bytes) {
        bool value = false;
        EXPECT_EQ(1u, in.read(value));
        // Compared through the object's own byte, which is defined for
        // every value a bool can hold and for the ones it cannot.
        unsigned char stored = 0xAA;
        std::memcpy(&stored, &value, 1);
        EXPECT_EQ(sent != 0 ? 1 : 0, (int)stored) << "sent " << (int)sent;
    }
    EXPECT_EQ(0u, in.length());
}

//////////////////////////////////////////////////////////////////////
// A repeated script parameter name is malformed input like any other,
// refused as a protocol error: the receive loops drop the connection on
// a ProtocolException and let everything else escape the zone thread.
//////////////////////////////////////////////////////////////////////

TEST(ScriptParameterTest, aRepeatedNameIsRefusedAsAProtocolError) {
    GCNPCAskVariable packet;
    EXPECT_THROW(readHandBuiltBody(packet,
                                   [](SocketOutputStream& out) {
                                       out.write((ObjectID_t)0x81A2B3C4);
                                       out.write((ScriptID_t)2);
                                       out.write((BYTE)2);
                                       for (int i = 0; i < 2; i++) {
                                           out.write((BYTE)1);
                                           out.write("a", 1);
                                           out.write((BYTE)1);
                                           out.write("b", 1);
                                       }
                                   }),
                 InvalidProtocolException);
}

//////////////////////////////////////////////////////////////////////
// A stall record has MAX_ITEM_NUM window slots. The item count comes off
// the wire as a byte, so one past the slots is refused before any item is
// read into a slot that is not there. On the gameserver the two
// store-info packets are refused before they are read and their factory
// packets hold no record, so this read is reached only where a caller
// has given the packet a record.
//////////////////////////////////////////////////////////////////////

namespace {

std::vector<unsigned char> storeBody(unsigned itemCount) {
    std::vector<unsigned char> body = {0x01, 0x01, 0x00, (unsigned char)itemCount};
    body.insert(body.end(), itemCount, 0x00); // every slot empty
    return body;
}

// GCMyStoreInfo keeps its record by pointer, so the test owns it.
struct MyStore {
    StoreInfo info;
    GCMyStoreInfo packet;
    MyStore() {
        packet.setStoreInfo(&info);
    }
};

void readFromMemory(Packet& packet, const std::vector<unsigned char>& body) {
    Socket socket(new SocketImpl());
    SocketInputStream in(&socket, (uint)body.size() + 1);
    ASSERT_TRUE(SocketInputStreamTestAccess::Preload(in, body.data(), body.size()));
    packet.read(in);
}

} // namespace

TEST(StoreInfoTest, anItemCountPastTheRecordsSlotsIsRefused) {
    auto store = std::make_unique<MyStore>();
    EXPECT_THROW(readFromMemory(store->packet, storeBody(MAX_ITEM_NUM + 1)), InvalidProtocolException);
    EXPECT_THROW(readFromMemory(store->packet, storeBody(255)), InvalidProtocolException);
}

TEST(StoreInfoTest, anItemCountThatFillsEverySlotIsRead) {
    auto store = std::make_unique<MyStore>();
    readFromMemory(store->packet, storeBody(MAX_ITEM_NUM));
    EXPECT_EQ((size_t)MAX_ITEM_NUM, store->info.getItems().size());
}
