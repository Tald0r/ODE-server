//////////////////////////////////////////////////////////////////////
//
// Filename    : packet_frame_test.cpp
// Description : SocketInputStream::readPacket reads one frame: the
//               seven-byte header and exactly the body size the header
//               declares. The packet's read() sees that body and nothing
//               past it, a read() that leaves a body byte unread or
//               fails on the way (even a failure it caught itself) is
//               refused with an InvalidProtocolException, and on every
//               exit the stream stands at the start of the next frame.
//
//               The cases use a stand-in packet whose read() does what
//               each case needs, and, for the goldens, every packet a
//               server's receive loop can read.
//
//////////////////////////////////////////////////////////////////////

#include <dirent.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Datagram.h"
#include "DatagramPacket.h"
#include "Exception.h"
#include "GCMyStoreInfo.h"
#include "GCOtherStoreInfo.h"
#include "Packet.h"
#include "PacketFactory.h"
#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketInputStreamTestAccess.h"
#include "StoreInfo.h"

#define ALL_PACKET_FACTORIES_INCLUDES
#include "AllPacketFactories.inc"
#undef ALL_PACKET_FACTORIES_INCLUDES

namespace {

typedef std::vector<unsigned char> Bytes;

// A packet whose read() runs whatever the case hands it.
class ScriptedPacket : public Packet {
public:
    using Packet::read;
    using Packet::write;

    explicit ScriptedPacket(std::function<void(SocketInputStream&)> body) : m_Body(std::move(body)) {}

    void read(SocketInputStream& iStream) override {
        m_Body(iStream);
    }
    void write(SocketOutputStream&) const override {}
    PacketID_t getPacketID() const override {
        return 0;
    }
    PacketSize_t getPacketSize() const override {
        return 0;
    }
    string getPacketName() const override {
        return "ScriptedPacket";
    }
    string toString() const override {
        return "ScriptedPacket";
    }

private:
    std::function<void(SocketInputStream&)> m_Body;
};

// One frame as the receive loops see it: id, declared body size,
// sequence byte, then the body bytes. `declared` defaults to the body's
// own length.
Bytes frame(PacketID_t id, const Bytes& body, long declared = -1, SequenceSize_t sequence = 0) {
    const PacketSize_t size = declared < 0 ? (PacketSize_t)body.size() : (PacketSize_t)declared;
    Bytes out(szPacketHeader);
    std::memcpy(&out[0], &id, szPacketID);
    std::memcpy(&out[szPacketID], &size, szPacketSize);
    std::memcpy(&out[szPacketID + szPacketSize], &sequence, szSequenceSize);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

Bytes concat(const Bytes& a, const Bytes& b) {
    Bytes out(a);
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

// The frame every case puts behind the one under test: four known bytes.
const Bytes kNextBody = {0xA1, 0xB2, 0xC3, 0xD4};

Bytes nextFrame() {
    return frame(0x1234, kNextBody, -1, 0x5A);
}

// Reads a body of exactly `n` bytes into `into`.
std::function<void(SocketInputStream&)> readBytes(uint n, Bytes* into = nullptr) {
    return [n, into](SocketInputStream& in) {
        Bytes got(n);
        in.read(reinterpret_cast<char*>(got.data()), n);
        if (into != nullptr)
            *into = got;
    };
}

// The stream stands at the start of nextFrame(), and that frame reads
// intact through readPacket, leaving the stream empty.
void expectNextFrameIntact(SocketInputStream& in) {
    ASSERT_EQ(szPacketHeader + kNextBody.size(), in.length()) << "the stream is not at the start of the next frame";

    char header[szPacketHeader];
    ASSERT_TRUE(in.peek(header, szPacketHeader));
    PacketID_t id = 0;
    PacketSize_t size = 0;
    std::memcpy(&id, &header[0], szPacketID);
    std::memcpy(&size, &header[szPacketID], szPacketSize);
    EXPECT_EQ(0x1234, id);
    EXPECT_EQ(kNextBody.size(), size);

    Bytes got;
    ScriptedPacket next(readBytes((uint)kNextBody.size(), &got));
    in.readPacket(&next);
    EXPECT_EQ(kNextBody, got);
    EXPECT_EQ(0u, in.length());
}

// A socketless stream loaded with `bytes` from the front of its buffer.
struct Stream {
    explicit Stream(const Bytes& bytes, uint capacity = 256) : socket(new SocketImpl()), in(&socket, capacity) {
        EXPECT_TRUE(SocketInputStreamTestAccess::Preload(in, bytes.data(), bytes.size()));
    }
    Socket socket;
    SocketEncryptInputStream in;
};

} // namespace

//////////////////////////////////////////////////////////////////////
// A read() that consumes exactly the declared body is read, and the next
// frame follows. A body of zero bytes is one too: the stream is at the
// next frame at once, and nothing skips a zero-byte remainder.
//////////////////////////////////////////////////////////////////////

TEST(FrameBoundTest, anExactReadIsAcceptedAndTheNextFrameFollows) {
    Stream s(concat(frame(7, {1, 2, 3}), nextFrame()));
    Bytes got;
    ScriptedPacket packet(readBytes(3, &got));
    s.in.readPacket(&packet);
    EXPECT_EQ(Bytes({1, 2, 3}), got);
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, anEmptyBodyIsAcceptedAndTheNextFrameFollows) {
    Stream s(concat(frame(7, {}), nextFrame()));
    bool sawEmpty = false;
    ScriptedPacket packet([&](SocketInputStream& in) { sawEmpty = in.length() == 0 && in.isEmpty(); });
    s.in.readPacket(&packet);
    EXPECT_TRUE(sawEmpty) << "read() of an empty body must see no bytes";
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// Before anything is consumed: the header and the whole declared body
// must be buffered, or readPacket throws InsufficientDataException and
// leaves every byte where it was, for the receive loop to try again once
// more has arrived.
//////////////////////////////////////////////////////////////////////

TEST(FrameBoundTest, aPartialHeaderConsumesNothing) {
    const Bytes whole = frame(7, {1, 2, 3});
    Stream s(Bytes(whole.begin(), whole.begin() + szPacketHeader - 1));
    bool ran = false;
    ScriptedPacket packet([&](SocketInputStream&) { ran = true; });
    EXPECT_THROW(s.in.readPacket(&packet), InsufficientDataException);
    EXPECT_FALSE(ran);
    EXPECT_EQ(szPacketHeader - 1, s.in.length());
}

TEST(FrameBoundTest, aPartialBodyConsumesNothing) {
    const Bytes whole = frame(7, {1, 2, 3});
    Stream s(Bytes(whole.begin(), whole.end() - 1));
    bool ran = false;
    ScriptedPacket packet([&](SocketInputStream& in) {
        ran = true;
        char body[3];
        in.read(body, 3);
    });
    EXPECT_THROW(s.in.readPacket(&packet), InsufficientDataException);
    EXPECT_FALSE(ran) << "read() must not run before the whole body is buffered";
    EXPECT_EQ(whole.size() - 1, s.in.length()) << "a fragmented frame must be left unconsumed";
}

//////////////////////////////////////////////////////////////////////
// A read() that leaves body bytes unread is refused, and the stream is
// at the next frame rather than inside the leftover body.
//////////////////////////////////////////////////////////////////////

TEST(FrameBoundTest, aTrailingBodyByteIsRefused) {
    Stream s(concat(frame(7, {1, 2, 3}), nextFrame()));
    ScriptedPacket packet(readBytes(2));
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aBodyLeftWhollyUnreadIsRefused) {
    Stream s(concat(frame(7, {1, 2, 3}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream&) {});
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// A field that runs past the declared body underflows it. The whole body
// was buffered before the read began, so that is a malformed body,
// refused with InvalidProtocolException, not InsufficientDataException:
// the receive loops take the latter to mean "wait for more bytes" and
// would retry a frame they had half consumed.
//////////////////////////////////////////////////////////////////////

TEST(FrameBoundTest, anUnderflowOfTheLastBufferedFrameIsAProtocolError) {
    // A one-byte length prefix announcing six bytes, three present.
    Stream s(frame(7, {6, 'a', 'b', 'c'}));
    ScriptedPacket packet([](SocketInputStream& in) {
        BYTE length = 0;
        in.read(length);
        string text;
        in.read(text, length);
    });
    try {
        s.in.readPacket(&packet);
        ADD_FAILURE() << "an underflowing body was accepted";
    } catch (InsufficientDataException&) {
        ADD_FAILURE() << "an underflow inside a buffered body was reported as fragmentation";
    } catch (InvalidProtocolException&) {
    }
    EXPECT_EQ(0u, s.in.length()) << "the refused frame must be consumed whole";
}

TEST(FrameBoundTest, anUnderflowWithAFrameBehindItIsAProtocolErrorAndTheNextFrameIsIntact) {
    Stream s(concat(frame(7, {6, 'a', 'b', 'c'}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream& in) {
        BYTE length = 0;
        in.read(length);
        string text;
        in.read(text, length);
    });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// No stream operation a read() makes can reach the next frame: not a
// read of any overload, not a peek, not a skip.
//////////////////////////////////////////////////////////////////////

TEST(FrameBoundTest, aScalarReadCannotCrossIntoTheNextFrame) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    DWORD crossed = 0;
    ScriptedPacket packet([&](SocketInputStream& in) { in.read(crossed); });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    EXPECT_EQ(0u, crossed) << "the read saw bytes of the next frame";
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aBufferReadCannotCrossIntoTheNextFrame) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream& in) {
        char buf[5];
        in.read(std::span<std::byte>(reinterpret_cast<std::byte*>(buf), sizeof(buf)));
    });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aStringReadCannotCrossIntoTheNextFrame) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    string crossed;
    ScriptedPacket packet([&](SocketInputStream& in) { in.read(crossed, 5); });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    EXPECT_TRUE(crossed.empty()) << "the read saw bytes of the next frame";
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aPeekCannotSeeTheNextFrame) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    bool saw = false;
    ScriptedPacket packet([&](SocketInputStream& in) {
        char buf[4] = {};
        saw = in.peek(buf, 4);
        char body[2];
        in.read(body, 2);
    });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    EXPECT_FALSE(saw) << "peek() returned bytes of the next frame";
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aPeekInsideTheBodyIsAllowed) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    Bytes peeked(2), got;
    ScriptedPacket packet([&](SocketInputStream& in) {
        ASSERT_TRUE(in.peek(reinterpret_cast<char*>(peeked.data()), 2));
        readBytes(2, &got)(in);
    });
    s.in.readPacket(&packet);
    EXPECT_EQ(Bytes({1, 2}), peeked);
    EXPECT_EQ(Bytes({1, 2}), got);
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aSkipCannotConsumeTheNextFrame) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream& in) { in.skip(3); });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aSkipOfTheWholeBodyIsAllowed) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream& in) { in.skip(2); });
    s.in.readPacket(&packet);
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// A read() that catches a stream failure and carries on is refused even
// when it then consumes the body exactly: the failure is recorded as it
// is thrown, where read() cannot hide it.
//////////////////////////////////////////////////////////////////////

TEST(FrameBoundTest, aSwallowedUnderflowIsRefused) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream& in) {
        try {
            DWORD tooWide = 0;
            in.read(tooWide);
        } catch (ProtocolException&) {
        }
        char body[2];
        in.read(body, 2);
    });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aSwallowedUnderflowOfTheLastBufferedFrameIsRefused) {
    Stream s(frame(7, {1, 2}));
    ScriptedPacket packet([](SocketInputStream& in) {
        try {
            DWORD tooWide = 0;
            in.read(tooWide);
        } catch (ProtocolException&) {
        }
        char body[2];
        in.read(body, 2);
    });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    EXPECT_EQ(0u, s.in.length());
}

TEST(FrameBoundTest, aSwallowedZeroLengthReadIsRefused) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream& in) {
        try {
            string nothing;
            in.read(nothing, 0);
        } catch (ProtocolException&) {
        }
        char body[2];
        in.read(body, 2);
    });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// A packet read inside another packet's read is refused without moving
// the stream, and the outer frame is refused with it.
//////////////////////////////////////////////////////////////////////

TEST(FrameBoundTest, aNestedReadPacketIsRefused) {
    // The outer body is itself a whole frame, so an unbounded nested read
    // would find a header there and read it.
    const Bytes inner = frame(9, {5, 6});
    Stream s(concat(frame(7, inner), nextFrame()));
    bool innerRefused = false;
    bool innerRan = false;
    uint lengthBefore = 0, lengthAfter = 0;
    ScriptedPacket packet([&](SocketInputStream& in) {
        ScriptedPacket nested([&](SocketInputStream& n) {
            innerRan = true;
            char body[2];
            n.read(body, 2);
        });
        lengthBefore = in.length();
        try {
            in.readPacket(&nested);
        } catch (InvalidProtocolException&) {
            innerRefused = true;
        }
        lengthAfter = in.length();
        // Consume the outer body exactly, so only the nesting is wrong.
        in.skip(in.length());
    });
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    EXPECT_TRUE(innerRefused);
    EXPECT_FALSE(innerRan);
    EXPECT_EQ(lengthBefore, lengthAfter) << "the refused nested read moved the stream";
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// Whatever read() throws, the stream is left at the next frame. A
// protocol exception that means something else to a receive loop is
// turned into InvalidProtocolException: IgnorePacketException makes the
// loops skip the frame, which readPacket has already consumed. Any other
// exception is passed on as it is.
//////////////////////////////////////////////////////////////////////

TEST(FrameBoundTest, anIgnorePacketExceptionFromReadIsAProtocolError) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream& in) {
        char one;
        in.read(&one, 1);
        throw IgnorePacketException();
    });
    try {
        s.in.readPacket(&packet);
        ADD_FAILURE() << "the frame was accepted";
    } catch (IgnorePacketException&) {
        ADD_FAILURE() << "an IgnorePacketException would make a receive loop skip the next frame";
    } catch (InvalidProtocolException&) {
    }
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, anyOtherExceptionFromReadLeavesTheStreamAtTheNextFrame) {
    Stream s(concat(frame(7, {1, 2}), nextFrame()));
    ScriptedPacket packet([](SocketInputStream& in) {
        char one;
        in.read(&one, 1);
        throw DisconnectException("read() gave up");
    });
    EXPECT_THROW(s.in.readPacket(&packet), DisconnectException);
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// The ring buffer's wrap-around: a frame whose body runs past the end of
// the buffer and on at its front is bounded the same way.
//////////////////////////////////////////////////////////////////////

namespace {

// A 32-byte ring with `bytes` loaded so the first frame's body straddles
// its end.
struct WrappedStream {
    explicit WrappedStream(const Bytes& bytes) : socket(new SocketImpl()), in(&socket, 32) {
        EXPECT_TRUE(SocketInputStreamTestAccess::PreloadAt(in, 32 - szPacketHeader - 2, bytes.data(), bytes.size()));
    }
    Socket socket;
    SocketInputStream in;
};

} // namespace

TEST(FrameBoundTest, aWrappedFrameReadsExactly) {
    WrappedStream s(concat(frame(7, {1, 2, 3, 4}), nextFrame()));
    Bytes got;
    ScriptedPacket packet(readBytes(4, &got));
    s.in.readPacket(&packet);
    EXPECT_EQ(Bytes({1, 2, 3, 4}), got);
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aWrappedFrameWithATrailingByteIsRefused) {
    WrappedStream s(concat(frame(7, {1, 2, 3, 4}), nextFrame()));
    ScriptedPacket packet(readBytes(3));
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

TEST(FrameBoundTest, aWrappedFrameCannotBeReadPast) {
    WrappedStream s(concat(frame(7, {1, 2, 3, 4}), nextFrame()));
    ScriptedPacket packet(readBytes(5));
    EXPECT_THROW(s.in.readPacket(&packet), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// The receive loops' shape: peek a header, wait for the whole body, read
// it. A refused frame ends the loop with InvalidProtocolException (every
// loop then drops its connection) and never leaves the stream inside a
// body, and a fragmented frame is waited for without being consumed.
//////////////////////////////////////////////////////////////////////

namespace {

// Mirrors what the four receive loops do with a frame: peek the header,
// wait until the whole body is buffered, then readPacket. Returns the
// bodies read, in order, and stops when fewer than one whole frame is
// buffered. GamePlayer's sequence gate, which runs before its length
// check, is not modelled here (docs/FIXES.md).
std::vector<Bytes> receive(SocketInputStream& in) {
    std::vector<Bytes> bodies;
    while (true) {
        char header[szPacketHeader];
        if (!in.peek(header, szPacketHeader))
            break;
        PacketSize_t size = 0;
        std::memcpy(&size, &header[szPacketID], szPacketSize);
        if (in.length() < szPacketHeader + size)
            break;
        // Every test body opens with the number of bytes read() takes.
        Bytes got;
        ScriptedPacket packet([&](SocketInputStream& s) {
            BYTE take = 0;
            s.read(take);
            got.assign(take, 0);
            if (take > 0)
                s.read(reinterpret_cast<char*>(got.data()), take);
        });
        in.readPacket(&packet);
        bodies.push_back(got);
    }
    return bodies;
}

} // namespace

TEST(FrameBoundTest, aReceiveLoopReadsFramesUntilOneIsFragmented) {
    const Bytes third = frame(7, {2, 9, 9});
    Stream s(concat(concat(frame(7, {1, 5}), frame(7, {0})), Bytes(third.begin(), third.end() - 1)));
    const std::vector<Bytes> bodies = receive(s.in);
    ASSERT_EQ(2u, bodies.size());
    EXPECT_EQ(Bytes({5}), bodies[0]);
    EXPECT_EQ(Bytes({}), bodies[1]);
    EXPECT_EQ(third.size() - 1, s.in.length()) << "the fragmented frame must wait whole";
}

TEST(FrameBoundTest, aReceiveLoopStopsAtARefusedFrameWithTheStreamAtTheNextOne) {
    // The second body declares one byte more than read() takes.
    Stream s(concat(concat(frame(7, {1, 5}), frame(7, {1, 6, 0xEE})), nextFrame()));
    std::vector<Bytes> bodies;
    EXPECT_THROW(bodies = receive(s.in), InvalidProtocolException);
    expectNextFrameIntact(s.in);
}

//////////////////////////////////////////////////////////////////////
// Every golden body of a packet some server's receive loop reads goes
// through readPacket as one frame and is consumed exactly, and the read
// packet declares the golden's length as its size.
//
// A receive loop reads a packet only if its server's factory table has
// it (tests/ratchet/factory_registrations.txt): the gameserver's
// SharedServerClient reads any id in that table, so every packet a
// gameserver, loginserver or sharedserver registers is covered. A
// datagram packet in those tables refuses a TCP stream with a
// ProtocolException, which must leave the stream at the next frame too;
// its golden is checked on the Datagram path it is really read from. The
// framed GCMoveOK golden carries its own header and is not a body.
//////////////////////////////////////////////////////////////////////

namespace {

std::map<std::string, PacketFactory*> factoriesByName() {
    std::vector<PacketFactory*> factories;
#define ALL_PACKET_FACTORIES_REGISTER
#include "AllPacketFactories.inc"
#undef ALL_PACKET_FACTORIES_REGISTER
    std::map<std::string, PacketFactory*> byName;
    for (PacketFactory* pFactory : factories)
        byName[pFactory->getPacketName()] = pFactory;
    return byName;
}

std::set<std::string> packetsAServerReads() {
    std::set<std::string> names;
    std::ifstream file(WIRETEST_REGISTRATIONS_FILE);
    EXPECT_TRUE(file.good()) << "cannot read " << WIRETEST_REGISTRATIONS_FILE;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream fields(line);
        std::string server, factory;
        if (!std::getline(fields, server, '\t') || !std::getline(fields, factory))
            continue;
        if (server != "gameserver" && server != "loginserver" && server != "sharedserver")
            continue;
        const std::string suffix = "Factory";
        if (factory.size() > suffix.size() &&
            factory.compare(factory.size() - suffix.size(), suffix.size(), suffix) == 0)
            names.insert(factory.substr(0, factory.size() - suffix.size()));
    }
    return names;
}

// Turns cout off for its lifetime.
struct QuietCout {
    QuietCout() : state(std::cout.rdstate()) {
        std::cout.setstate(std::ios_base::badbit);
    }
    ~QuietCout() {
        std::cout.clear(state);
    }
    std::ios_base::iostate state;
};

Bytes readHex(const std::string& path) {
    std::ifstream file(path.c_str());
    EXPECT_TRUE(file.good()) << "cannot read " << path;
    std::string hex, line;
    while (std::getline(file, line))
        for (char c : line)
            if (!std::isspace((unsigned char)c))
                hex.push_back(c);
    Bytes bytes;
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
        bytes.push_back((unsigned char)strtoul(hex.substr(i, 2).c_str(), NULL, 16));
    return bytes;
}

} // namespace

namespace {

struct GoldenCounts {
    size_t frames = 0;
    size_t datagrams = 0;
    std::set<std::string> packets;
};

// A datagram packet refuses a TCP stream outright; the servers read it
// from a Datagram, one packet per datagram. Its golden is checked on that
// path: the body reads without running off the datagram, the read packet
// writes the same bytes back, and it declares their number as its size.
void expectDatagramGoldenReads(const std::string& file, DatagramPacket& packet, const Bytes& body) {
    std::vector<char> raw(body.begin(), body.end());
    Datagram in;
    in.setData(raw.data(), (uint)raw.size());
    try {
        packet.read(in);
    } catch (Throwable& t) {
        ADD_FAILURE() << file << ": the datagram read refused: " << t.toString();
        return;
    }
    Datagram out;
    out.write(&packet);
    ASSERT_LE(szPacketHeader, out.getLength()) << file;
    const char* pBody = out.getData() + szPacketID + szPacketSize;
    EXPECT_EQ(body, Bytes(pBody, pBody + (out.getLength() - szPacketHeader)))
        << file << ": the read packet writes different bytes";
    EXPECT_EQ(body.size(), (size_t)packet.getPacketSize()) << file << ": getPacketSize() of the read packet";
}

void expectGoldenReadsAsOneFrame(const std::string& file, PacketFactory& factory, uchar code, GoldenCounts& counts) {
    const Bytes body = readHex(std::string(WIRETEST_GOLDEN_DIR) + "/" + file);
    Socket socket(new SocketImpl());
    SocketEncryptInputStream in(&socket, (uint)(body.size() + 64 + szPacketHeader * 2));
    in.setEncryptCode(code);
    const Bytes bytes = concat(frame(factory.getPacketID(), body, -1, 0x5A), nextFrame());
    ASSERT_TRUE(SocketInputStreamTestAccess::Preload(in, bytes.data(), bytes.size()));

    std::unique_ptr<Packet> pPacket(factory.createPacket());
    // The store-info packets read into a record the sender owns, and a
    // packet the factory makes holds none.
    StoreInfo store;
    if (GCMyStoreInfo* pMine = dynamic_cast<GCMyStoreInfo*>(pPacket.get()))
        pMine->setStoreInfo(&store);
    if (GCOtherStoreInfo* pOther = dynamic_cast<GCOtherStoreInfo*>(pPacket.get()))
        pOther->setStoreInfo(&store);
    DatagramPacket* pDatagram = dynamic_cast<DatagramPacket*>(pPacket.get());

    try {
        // readPacket prints every packet it reads; six hundred of them
        // would bury the test's own output.
        QuietCout quiet;
        in.readPacket(pPacket.get());
        EXPECT_EQ(nullptr, pDatagram) << file << ": a datagram packet was read from a TCP stream";
    } catch (ProtocolException& e) {
        if (pDatagram == nullptr) {
            ADD_FAILURE() << file << ": refused: " << e.toString();
            return;
        }
    } catch (Throwable& t) {
        ADD_FAILURE() << file << ": refused with a non-protocol exception: " << t.toString();
        return;
    }
    // Read or refused, the stream is at the next frame.
    const size_t consumed = bytes.size() - in.length();
    EXPECT_EQ(szPacketHeader + body.size(), consumed)
        << file << ": the frame declared " << body.size() << " body bytes and readPacket consumed "
        << (long)consumed - (long)szPacketHeader;

    if (pDatagram != nullptr) {
        std::unique_ptr<Packet> pFresh(factory.createPacket());
        expectDatagramGoldenReads(file, *dynamic_cast<DatagramPacket*>(pFresh.get()), body);
        counts.datagrams++;
    } else {
        EXPECT_EQ(body.size(), (size_t)pPacket->getPacketSize()) << file << ": getPacketSize() of the read packet";
        counts.frames++;
    }
    counts.packets.insert(factory.getPacketName());
}

} // namespace

TEST(FrameBoundTest, everyGoldenAServerReadsIsConsumedExactlyAsOneFrame) {
    const std::map<std::string, PacketFactory*> byName = factoriesByName();
    const std::set<std::string> read = packetsAServerReads();
    ASSERT_FALSE(read.empty());

    DIR* dir = opendir(WIRETEST_GOLDEN_DIR);
    ASSERT_NE(nullptr, dir);
    std::vector<std::string> files;
    while (struct dirent* entry = readdir(dir)) {
        const std::string file = entry->d_name;
        if (file.size() > 4 && file.compare(file.size() - 4, 4, ".hex") == 0)
            files.push_back(file);
    }
    closedir(dir);
    std::sort(files.begin(), files.end());

    GoldenCounts counts;
    for (const std::string& file : files) {
        const std::string base = file.substr(0, file.size() - 4);
        const std::string name = base.substr(0, base.find('.'));
        if (read.count(name) == 0 || base.find(".framed.") != std::string::npos)
            continue;
        const size_t codeAt = base.rfind(".code");
        ASSERT_NE(std::string::npos, codeAt) << file;
        const uchar code = (uchar)std::atoi(base.c_str() + codeAt + 5);

        const std::map<std::string, PacketFactory*>::const_iterator factory = byName.find(name);
        ASSERT_NE(byName.end(), factory) << file << ": no factory named " << name;
        expectGoldenReadsAsOneFrame(file, *factory->second, code, counts);
    }
    // The counts when this was written; a golden dropped from a packet a
    // server reads shows up here.
    EXPECT_EQ(653u, counts.frames);
    EXPECT_EQ(14u, counts.datagrams);
    EXPECT_EQ(449u, counts.packets.size());
}
