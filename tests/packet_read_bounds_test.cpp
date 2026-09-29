//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_read_bounds_test.cpp
// Description : Malformed bodies the packet-read fuzz targets
//               (tests/fuzz/) found reaching memory they must not, each
//               pinned here as the refusal or the value the read now
//               gives. The inputs that found them are replayed as well,
//               from tests/fuzz/regressions/, by the fuzz_replay tests.
//
//               Every case is a body a client can put on the wire: in
//               GPS_NORMAL the gameserver reads any packet it registers,
//               and every packet it reads is printed with toString().
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
