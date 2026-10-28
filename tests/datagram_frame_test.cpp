//////////////////////////////////////////////////////////////////////
//
// Filename    : datagram_frame_test.cpp
// Description : Pins the UDP framing contract of Datagram: the size
//               field carries the number of bytes the body actually
//               wrote, the datagram's length matches it, and a body
//               that outgrows the buffer its declared size bought is
//               still sent whole.
//
// The server-to-server UDP link has no goldens, because a datagram
// packet cannot be written to a socket stream at all. These tests are
// its equivalent: the frame is checked byte for byte, and one packet
// makes a real loopback trip through the receive path that
// reconstructs it.
//
//////////////////////////////////////////////////////////////////////

#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "CGPortCheck.h"
#include "Datagram.h"
#include "DatagramPacket.h"
#include "DatagramSocket.h"
#include "Exception.h"
#include "GLIncomingConnectionError.h"
#include "GMServerInfo.h"
#include "KernelContext.h"
#include "LGIncomingConnectionError.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "Types.h"

namespace {

// A packet whose declared size can disagree with what its write()
// produces, in either direction -- the datagram twin of the TCP
// stream's DriftingPacket.
class DriftingDatagramPacket : public DatagramPacket {
public:
    DriftingDatagramPacket(uint bodySize, int declaredDrift) : m_BodySize(bodySize), m_DeclaredDrift(declaredDrift) {}

    void read(Datagram& iDatagram) {
        throw UnsupportedError();
    }

    void write(Datagram& oDatagram) const {
        // One field at a time: a body that overruns its buffer must grow
        // it through the same path every packet writes through.
        for (uint i = 0; i < m_BodySize; i++)
            oDatagram.write((BYTE)(0xA0 + i));
    }

    PacketID_t getPacketID() const {
        return 0x4321;
    }
    PacketSize_t getPacketSize() const {
        return (PacketSize_t)((int)m_BodySize + m_DeclaredDrift);
    }
    string getPacketName() const {
        return "DriftingDatagramPacket";
    }
    string toString() const {
        return "DriftingDatagramPacket";
    }

private:
    uint m_BodySize;
    int m_DeclaredDrift;
};

// The size field as the receiver reads it: little-endian, at the
// datagram's own header offset.
unsigned int sizeField(Datagram& datagram) {
    const unsigned char* pBuffer = (const unsigned char*)datagram.getData();
    unsigned int size = 0;
    for (uint i = 0; i < szPacketSize; i++)
        size |= (unsigned int)pBuffer[szPacketID + i] << (8 * i);
    return size;
}

// The receive path builds packets through the factory table registered
// on the kernel context, which no other test in this binary installs.
void installPortCheckFactory() {
    static PacketFactoryManager* pPacketFactoryManager = NULL;
    if (pPacketFactoryManager == NULL) {
        pPacketFactoryManager = new PacketFactoryManager();
        pPacketFactoryManager->addFactory(new CGPortCheckFactory());
        de::kernelContext().setPacketFactoryManager(pPacketFactoryManager);
    }
}

// Frames a packet and returns the body its write() produced. The size
// field, the datagram's length and the packet's own getPacketSize() must
// all name the same number of bytes, and that number must fit the body
// size its factory advertises -- the receiver drops a datagram whose
// size field exceeds the factory's maximum.
std::vector<char> frameBody(const DatagramPacket& packet, PacketSize_t maxSize, const std::string& what) {
    Datagram datagram;
    datagram.write(&packet);

    EXPECT_LE(szPacketHeader, datagram.getLength()) << what;
    const uint measured = datagram.getLength() - szPacketHeader;

    EXPECT_EQ((uint)packet.getPacketSize(), measured) << what;
    EXPECT_EQ(measured, sizeField(datagram)) << what;
    EXPECT_LE(measured, (uint)maxSize) << what;

    const char* pBody = datagram.getData() + szPacketID + szPacketSize;
    return std::vector<char>(pBody, pBody + measured);
}

// Reads a packet back from a buffer holding nothing but the body, so a
// read() that consumes more than write() produced runs off the end of
// the datagram rather than into a neighbouring field.
void readBody(std::vector<char>& body, DatagramPacket& packet) {
    Datagram datagram;
    datagram.setData(body.data(), body.size());
    packet.read(datagram);
}

// The two IncomingConnectionError packets are the same layout on two
// packet ids, so they are pinned by one body.
template <typename P> void expectIncomingConnectionErrorIsMeasured(PacketSize_t maxSize, const std::string& what) {
    // Both writers reject a field of 128 bytes or more, so 127 and 127
    // is the largest body either can put on the wire.
    EXPECT_EQ(szBYTE + 127 + szBYTE + 127, (uint)maxSize) << what;

    for (uint length : {1u, 40u, 127u}) {
        const std::string message(length, 'm');
        const std::string playerID(length, 'p');

        P packet;
        packet.setMessage(message);
        packet.setPlayerID(playerID);

        std::vector<char> body = frameBody(packet, maxSize, what + ", length " + std::to_string(length));
        ASSERT_EQ(szBYTE + length + szBYTE + length, body.size()) << what << ", length " << length;

        P rebuilt;
        readBody(body, rebuilt);
        EXPECT_EQ(message, rebuilt.getMessage()) << what << ", length " << length;
        EXPECT_EQ(playerID, rebuilt.getPlayerID()) << what << ", length " << length;
    }
}

} // namespace

// A lie in either direction still frames the body that was written.
TEST(DatagramFrameSize, TheHeaderCarriesTheMeasuredBodyLength) {
    const uint kBodySize = 9;

    for (int drift : {-4, 0, 7}) {
        DriftingDatagramPacket packet(kBodySize, drift);
        Datagram datagram;
        datagram.write(&packet);

        EXPECT_EQ(kBodySize, sizeField(datagram)) << "drift " << drift;
        ASSERT_EQ(szPacketHeader + kBodySize, datagram.getLength()) << "drift " << drift;

        const unsigned char* pBuffer = (const unsigned char*)datagram.getData();
        for (uint i = 0; i < kBodySize; i++)
            EXPECT_EQ(0xA0 + i, pBuffer[szPacketID + szPacketSize + i]) << "drift " << drift << ", body byte " << i;
    }
}

// An honest packet's datagram is byte for byte what it always was: id,
// size, body, and the one trailing pad byte both peers count into the
// datagram's length but neither reads.
TEST(DatagramFrameSize, AnHonestPacketsDatagramIsUnchanged) {
    ASSERT_EQ(2u, szPacketID);
    ASSERT_EQ(4u, szPacketSize);
    ASSERT_EQ(7u, szPacketHeader);

    DriftingDatagramPacket packet(4, 0);
    Datagram datagram;
    datagram.write(&packet);

    const unsigned char expected[] = {0x21, 0x43, 0x04, 0x00, 0x00, 0x00, 0xA0, 0xA1, 0xA2, 0xA3, 0x00};
    ASSERT_EQ(sizeof(expected), datagram.getLength());
    EXPECT_EQ(0, memcmp(expected, datagram.getData(), sizeof(expected)));
}

// A body larger than the buffer its declared size bought grows the
// buffer and is sent whole, rather than aborting the write half way
// through a partly filled datagram.
TEST(DatagramFrameSize, ABodyLargerThanTheDeclaredSizeIsFramedWhole) {
    const uint kBodySize = 200;

    // Declared sizes from "this packet writes nothing" up to one byte
    // short, so the growth path runs from an empty buffer and from a
    // nearly full one.
    for (uint declared : {0u, 1u, 64u, kBodySize - 1}) {
        DriftingDatagramPacket packet(kBodySize, (int)declared - (int)kBodySize);
        Datagram datagram;
        datagram.write(&packet);

        EXPECT_EQ(kBodySize, sizeField(datagram)) << "declared " << declared;
        ASSERT_EQ(szPacketHeader + kBodySize, datagram.getLength()) << "declared " << declared;

        const unsigned char* pBuffer = (const unsigned char*)datagram.getData();
        EXPECT_EQ(0x21, pBuffer[0]);
        EXPECT_EQ(0x43, pBuffer[1]);
        for (uint i = 0; i < kBodySize; i++)
            EXPECT_EQ((unsigned char)(0xA0 + i), pBuffer[szPacketID + szPacketSize + i])
                << "declared " << declared << ", body byte " << i;
        // The pad the length counts is written, not left as whatever the
        // allocation happened to hold.
        EXPECT_EQ(0x00, pBuffer[datagram.getLength() - 1]) << "declared " << declared;
    }
}

// GMServerInfo carries a variable-length zone table, so its declared
// size is the one most easily left behind by an edit to write(). It goes
// to the login server every ten seconds, which makes a drift here a
// steady stream of report lines.
TEST(DatagramDeclaredSize, GMServerInfoDeclaresItsZoneTable) {
    // The zone count is a BYTE, so 255 zones is the largest table.
    for (uint zones : {0u, 1u, 5u, 255u}) {
        const std::string what = "zones " + std::to_string(zones);

        GMServerInfo packet;
        packet.setWorldID(3);
        packet.setServerID(7);
        for (uint i = 0; i < zones; i++)
            packet.addZoneUserData((ZoneID_t)(1000 + i), 40 + i);

        std::vector<char> body = frameBody(packet, GMServerInfoFactory::kMaxSize, what);

        // WorldID, ServerID and the zone count, then a ZoneID and a WORD
        // user count for each zone.
        ASSERT_EQ(szWorldID + szBYTE + szBYTE + zones * (szZoneID + szWORD), body.size()) << what;

        GMServerInfo rebuilt;
        readBody(body, rebuilt);
        EXPECT_EQ(3u, (uint)rebuilt.getWorldID()) << what;
        EXPECT_EQ(7u, (uint)rebuilt.getServerID()) << what;
        ASSERT_EQ(zones, (uint)rebuilt.getZoneUserCount()) << what;

        for (uint i = 0; i < zones; i++) {
            ZONEUSERDATA zoneUserData;
            rebuilt.popZoneUserData(zoneUserData);
            EXPECT_EQ((ZoneID_t)(1000 + i), zoneUserData.ZoneID) << what << ", zone " << i;
            EXPECT_EQ((WORD)(40 + i), zoneUserData.UserNum) << what << ", zone " << i;
        }
    }
}

TEST(DatagramDeclaredSize, GLIncomingConnectionErrorDeclaresBothStrings) {
    expectIncomingConnectionErrorIsMeasured<GLIncomingConnectionError>(GLIncomingConnectionErrorFactory::kMaxSize,
                                                                       "GLIncomingConnectionError");
}

TEST(DatagramDeclaredSize, LGIncomingConnectionErrorDeclaresBothStrings) {
    expectIncomingConnectionErrorIsMeasured<LGIncomingConnectionError>(LGIncomingConnectionErrorFactory::kMaxSize,
                                                                       "LGIncomingConnectionError");
}

// The whole path, over a real UDP socket pair: a packet is framed,
// sent, received, and rebuilt by the factory-backed read side.
TEST(DatagramFrameRoundTrip, LoopbackRebuildsThePacket) {
    installPortCheckFactory();

    // Port 0: the kernel picks a free port, so parallel runs of this
    // suite cannot collide.
    DatagramSocket receiver(0);

    SOCKADDR_IN bound;
    memset(&bound, 0, sizeof(bound));
    socklen_t boundLen = sizeof(bound);
    ASSERT_EQ(0, getsockname(receiver.getSOCKET(), (SOCKADDR*)&bound, &boundLen));

    // Without this a lost datagram would hang the case until ctest's
    // timeout instead of failing.
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    ASSERT_EQ(0, setsockopt(receiver.getSOCKET(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));

    CGPortCheck sent;
    sent.setPCName("Cheiron");

    Datagram outgoing;
    outgoing.setHost("127.0.0.1");
    outgoing.setPort(ntohs(bound.sin_port));
    outgoing.write(&sent);

    ASSERT_EQ(szPacketHeader + (uint)sent.getPacketSize(), outgoing.getLength());

    DatagramSocket sender(0);
    ASSERT_EQ(outgoing.getLength(), sender.send(&outgoing));

    Datagram* pIncoming = receiver.receive();
    ASSERT_TRUE(pIncoming != NULL) << "no datagram arrived on the loopback";
    EXPECT_EQ(outgoing.getLength(), pIncoming->getLength());

    DatagramPacket* pReceived = NULL;
    pIncoming->read(pReceived);
    ASSERT_TRUE(pReceived != NULL);
    EXPECT_EQ((PacketID_t)Packet::PACKET_CG_PORT_CHECK, pReceived->getPacketID());
    EXPECT_EQ("Cheiron", ((CGPortCheck*)pReceived)->getPCName());

    delete pReceived;
    delete pIncoming;
}

// The receiver's length check is the other half of the measured size
// field: a datagram that does not hold exactly the packet its header
// claims is rejected rather than read past.
TEST(DatagramFrameRoundTrip, ALengthThatDisagreesWithTheSizeFieldIsRejected) {
    installPortCheckFactory();

    CGPortCheck packet;
    packet.setPCName("Cheiron");

    Datagram framed;
    framed.write(&packet);

    for (int lengthDrift : {-1, 1}) {
        std::vector<char> bytes(framed.getLength() + 1, 0);
        memcpy(bytes.data(), framed.getData(), framed.getLength());

        Datagram damaged;
        damaged.setData(bytes.data(), framed.getLength() + lengthDrift);

        DatagramPacket* pReceived = NULL;
        EXPECT_THROW(damaged.read(pReceived), Error) << "length drift " << lengthDrift;
        EXPECT_TRUE(pReceived == NULL) << "length drift " << lengthDrift;
    }
}

// A read that wants more than the datagram still holds refuses it with a
// ProtocolException, as a stream read past its frame does, and leaves the
// read position where it was: a body that runs past its datagram is
// turned away like any malformed body.
TEST(DatagramFrameRoundTrip, AReadPastTheDatagramIsAProtocolRefusal) {
    char bytes[3] = {1, 2, 3};
    Datagram datagram;
    datagram.setData(bytes, sizeof(bytes));

    char out[4] = {};
    EXPECT_THROW(datagram.read(out, 4), InsufficientDataException);
    std::string text;
    EXPECT_THROW(datagram.read(text, 4), InsufficientDataException);

    datagram.read(out, 3);
    EXPECT_EQ(0, memcmp(out, bytes, 3));
    EXPECT_THROW(datagram.read(out, 1), InsufficientDataException);
}
