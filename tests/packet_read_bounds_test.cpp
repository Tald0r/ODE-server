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
