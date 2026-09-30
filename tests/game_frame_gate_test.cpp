//////////////////////////////////////////////////////////////////////
//
// Filename    : game_frame_gate_test.cpp
// Description : GameFrameGate, the per-frame decision in
//               GamePlayer::processCommand: wait for more bytes, read an
//               admitted packet, skip an ignored one, or refuse the
//               frame. Compiled as the gameserver (__GAME_SERVER__) and
//               linked with GameServerPackets, so the factory table and
//               the validator are the gameserver's own. The bytes reach
//               the input stream the way fill() puts them there, in the
//               chunks each case chooses, so a frame can arrive in parts
//               the way TCP may deliver it.
//
//////////////////////////////////////////////////////////////////////

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "CGMove.h"
#include "GameFrameGate.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PlayerStatus.h"
#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketImpl.h"
#include "SocketInputStreamTestAccess.h"

namespace {

using de::GameFrame;
using de::GameFrameGate;
using de::GameFrameRefusal;
using de::GameFrameStep;

typedef std::vector<unsigned char> Bytes;

// A frame as a client puts it on the wire: the id, the body size and the
// sequence byte, then the body.
Bytes frameBytes(PacketID_t id, SequenceSize_t sequence, const Bytes& body, PacketSize_t size) {
    Bytes out(szPacketHeader);
    std::memcpy(&out[0], &id, szPacketID);
    std::memcpy(&out[szPacketID], &size, szPacketSize);
    std::memcpy(&out[szPacketID + szPacketSize], &sequence, szSequenceSize);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

Bytes frameBytes(PacketID_t id, SequenceSize_t sequence, const Bytes& body) {
    return frameBytes(id, sequence, body, (PacketSize_t)body.size());
}

// CGMove's body under encrypt code 0: the direction, then x and y.
Bytes moveFrame(SequenceSize_t sequence, BYTE x = 10, BYTE y = 20, BYTE dir = 3) {
    return frameBytes(Packet::PACKET_CG_MOVE, sequence, Bytes{dir, x, y});
}

// CGReady has an empty body.
Bytes readyFrame(SequenceSize_t sequence) {
    return frameBytes(Packet::PACKET_CG_READY, sequence, Bytes{});
}

Bytes concat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& part : parts)
        out.insert(out.end(), part.begin(), part.end());
    return out;
}

class GameFrameGateTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_Factories.init();
        m_Validator.init();
        m_In.setEncryptCode(0);
    }

    // Hands the stream bytes as one receive would.
    void deliver(const Bytes& bytes) {
        ASSERT_TRUE(SocketInputStreamTestAccess::Append(m_In, bytes.data(), bytes.size()));
    }

    GameFrame next(PlayerStatus status = GPS_NORMAL) {
        return m_Gate.next(m_In, status, m_Factories, m_Validator);
    }

    PacketFactoryManager m_Factories;
    PacketValidator m_Validator;
    Socket m_Socket{new SocketImpl()};
    SocketEncryptInputStream m_In{&m_Socket, 4096};
    GameFrameGate m_Gate;
};

TEST_F(GameFrameGateTest, waitsForAHeader) {
    EXPECT_EQ(GameFrameStep::AwaitHeader, next().step);
    deliver(Bytes{0x01, 0x02, 0x03});
    EXPECT_EQ(GameFrameStep::AwaitHeader, next().step);
    EXPECT_EQ(0u, (unsigned)m_Gate.expectedSequence());
    EXPECT_EQ(3u, m_In.length());
}

TEST_F(GameFrameGateTest, readsAWholeFrameAndCountsItsSequence) {
    deliver(moveFrame(0, 11, 22, 5));

    GameFrame frame = next();
    ASSERT_EQ(GameFrameStep::Read, frame.step);
    EXPECT_EQ(GameFrameRefusal::None, frame.refusal);
    EXPECT_EQ(Packet::PACKET_CG_MOVE, frame.id);
    EXPECT_EQ(3u, frame.size);
    const CGMove* move = dynamic_cast<const CGMove*>(frame.packet.get());
    ASSERT_NE(nullptr, move);
    EXPECT_EQ(11, move->getX());
    EXPECT_EQ(22, move->getY());
    EXPECT_EQ(5, move->getDir());

    EXPECT_EQ(1u, (unsigned)m_Gate.expectedSequence());
    EXPECT_EQ(0u, m_In.length());
    EXPECT_EQ(GameFrameStep::AwaitHeader, next().step);
}

TEST_F(GameFrameGateTest, readsSeveralWholeFramesFromOneReceive) {
    deliver(concat({moveFrame(0), readyFrame(1), moveFrame(2), readyFrame(3)}));

    std::vector<PacketID_t> read;
    while (true) {
        GameFrame frame = next();
        if (frame.step != GameFrameStep::Read) {
            EXPECT_EQ(GameFrameStep::AwaitHeader, frame.step);
            break;
        }
        read.push_back(frame.packet->getPacketID());
    }
    EXPECT_EQ((std::vector<PacketID_t>{Packet::PACKET_CG_MOVE, Packet::PACKET_CG_READY, Packet::PACKET_CG_MOVE,
                                       Packet::PACKET_CG_READY}),
              read);
    EXPECT_EQ(4u, (unsigned)m_Gate.expectedSequence());
}

TEST_F(GameFrameGateTest, refusesAWholeFrameOutOfSequence) {
    deliver(moveFrame(1));

    GameFrame frame = next();
    EXPECT_EQ(GameFrameStep::Refused, frame.step);
    EXPECT_EQ(GameFrameRefusal::OutOfSequence, frame.refusal);
    EXPECT_EQ(1u, (unsigned)frame.sequence);
    // Refused unread: the frame is still in the stream.
    EXPECT_EQ(szPacketHeader + 3, m_In.length());
}

TEST_F(GameFrameGateTest, refusesARepeatedSequence) {
    deliver(concat({moveFrame(0), moveFrame(0)}));

    EXPECT_EQ(GameFrameStep::Read, next().step);
    GameFrame frame = next();
    EXPECT_EQ(GameFrameStep::Refused, frame.step);
    EXPECT_EQ(GameFrameRefusal::OutOfSequence, frame.refusal);
}

TEST_F(GameFrameGateTest, theSequenceWrapsAfter255) {
    Bytes stream;
    for (int i = 0; i < 300; i++) {
        const Bytes frame = readyFrame((SequenceSize_t)(i & 0xFF));
        stream.insert(stream.end(), frame.begin(), frame.end());
    }
    deliver(stream);

    int read = 0;
    while (next().step == GameFrameStep::Read)
        read++;
    EXPECT_EQ(300, read);
    EXPECT_EQ(300 % 256, (int)m_Gate.expectedSequence());
}

TEST_F(GameFrameGateTest, refusesAnIdPastTheTable) {
    deliver(frameBytes((PacketID_t)Packet::PACKET_MAX, 0, Bytes{}));

    GameFrame frame = next();
    EXPECT_EQ(GameFrameStep::Refused, frame.step);
    EXPECT_EQ(GameFrameRefusal::IdOutOfRange, frame.refusal);
}

TEST_F(GameFrameGateTest, refusesAPacketTheStatusDoesNotAdmit) {
    // A client sends no GCMoveOK, so the in-game set does not admit it.
    deliver(frameBytes(Packet::PACKET_GC_MOVE_OK, 0, Bytes{}));

    GameFrame frame = next();
    EXPECT_EQ(GameFrameStep::Refused, frame.step);
    EXPECT_EQ(GameFrameRefusal::InvalidOrder, frame.refusal);
}

TEST_F(GameFrameGateTest, refusesABodyLargerThanThePacketsMaximum) {
    const PacketSize_t tooLarge = m_Factories.getPacketMaxSize(Packet::PACKET_CG_MOVE) + 1;
    deliver(frameBytes(Packet::PACKET_CG_MOVE, 0, Bytes{}, tooLarge));

    GameFrame frame = next();
    EXPECT_EQ(GameFrameStep::Refused, frame.step);
    EXPECT_EQ(GameFrameRefusal::TooLarge, frame.refusal);
}

// GPS_WAITING_FOR_CG_READY is a PIST_IGNORE_EXCEPT status: it admits
// CGReady and the two hot-key packets and ignores the rest.
TEST_F(GameFrameGateTest, skipsAWholeIgnoredFrameAndCountsItsSequence) {
    deliver(concat({moveFrame(0), readyFrame(1)}));

    GameFrame ignored = next(GPS_WAITING_FOR_CG_READY);
    EXPECT_EQ(GameFrameStep::Skipped, ignored.step);
    EXPECT_EQ(Packet::PACKET_CG_MOVE, ignored.id);
    EXPECT_EQ(nullptr, ignored.packet);
    EXPECT_EQ(1u, (unsigned)m_Gate.expectedSequence());

    GameFrame ready = next(GPS_WAITING_FOR_CG_READY);
    ASSERT_EQ(GameFrameStep::Read, ready.step);
    EXPECT_EQ(Packet::PACKET_CG_READY, ready.packet->getPacketID());
    EXPECT_EQ(2u, (unsigned)m_Gate.expectedSequence());
}

TEST_F(GameFrameGateTest, refusesAnIgnoredFrameOutOfSequence) {
    deliver(moveFrame(1));

    GameFrame frame = next(GPS_WAITING_FOR_CG_READY);
    EXPECT_EQ(GameFrameStep::Refused, frame.step);
    EXPECT_EQ(GameFrameRefusal::OutOfSequence, frame.refusal);
}

TEST_F(GameFrameGateTest, refusesAnIgnoredBodyLargerThanThePacketsMaximum) {
    const PacketSize_t tooLarge = m_Factories.getPacketMaxSize(Packet::PACKET_CG_MOVE) + 1;
    deliver(frameBytes(Packet::PACKET_CG_MOVE, 0, Bytes{}, tooLarge));

    GameFrame frame = next(GPS_WAITING_FOR_CG_READY);
    EXPECT_EQ(GameFrameStep::Refused, frame.step);
    EXPECT_EQ(GameFrameRefusal::IgnoredTooLarge, frame.refusal);
}

} // namespace
