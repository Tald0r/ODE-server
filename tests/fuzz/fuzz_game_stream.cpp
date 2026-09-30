//////////////////////////////////////////////////////////////////////
//
// Filename    : fuzz_game_stream.cpp
// Description : Packet-read fuzz target for the gameserver's client
//               connection. It mirrors GameFrameGate::next
//               (src/server/gameserver/GameFrameGate.cpp), the gate
//               GamePlayer::processCommand runs each frame through, up to
//               the point where the packet has been read, and stops short
//               of the handler: every gate that decides whether a
//               client's bytes reach a packet's read() is here, in
//               production order, and nothing after it.
//
//               Built with __GAME_SERVER__ and linked with
//               GameServerPackets, so the factory table is the
//               gameserver's own: Concat<GameOnlyFactories,
//               ClientLinkFactories, GuildLinkFactories>. The validator
//               is the gameserver's too, so in GPS_NORMAL it admits only
//               the packets a client sends (GameClientLink.h) and refuses
//               the rest of that table before any read.
//               DE_FUZZ_ANY_ID=1 admits every id in GPS_NORMAL instead,
//               so the reads behind the gate are fuzzed as well.
//
//               The input format, the three deliveries and the switches
//               are described in StreamFuzz.h. The loop runs the zone
//               thread's call (Option == true), so the main-thread stop
//               after CGReady and the event heartbeat do not apply.
//
//////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketImpl.h"
#include "StreamFuzz.h"

// tests/fuzz/CMakeLists.txt prefixes the golden seeds with this status
// byte, so a renumbering must move the seeds with it.
static_assert(GPS_NORMAL == 3, "the game seed corpus is written for GPS_NORMAL == 3");

namespace {

// What the gameserver keeps per connection between two receives.
struct Session {
    // The sequence byte the next frame must carry; GameFrameGate starts
    // at 0.
    SequenceSize_t expectedSequence = 0;
    // Frames consumed, read or skipped.
    int frames = 0;
};

// Runs the gate over what the stream holds, as processCommand does after
// each receive: returns when it waits for more bytes, and ends the trace
// when the gate refuses a frame or kMaxFrames have been consumed.
void receive(SocketInputStream& in, PlayerStatus status, Session& session, de::fuzz::Trace& trace) {
    PacketFactoryManager& factories = de::kernelContext().packetFactories();
    PacketValidator& validator = de::kernelContext().packetValidator();

    while (true) {
        if (session.frames == de::fuzz::kMaxFrames) {
            trace.end = "frame cap";
            return;
        }

        char header[szPacketHeader];
        // Fewer than seven bytes buffered: wait for more.
        if (!in.peek(&header[0], szPacketHeader))
            return;

        PacketID_t packetID;
        PacketSize_t packetSize;
        SequenceSize_t packetSequence;
        std::memcpy(&packetID, &header[0], szPacketID);
        std::memcpy(&packetSize, &header[szPacketID], szPacketSize);
        std::memcpy(&packetSequence, &header[szPacketID + szPacketSize], szSequenceSize);

        if (packetID >= (int)Packet::PACKET_MAX) {
            trace.end = "too large packet id";
            return;
        }

        try {
            // A PIST_IGNORE_EXCEPT status throws IgnorePacketException
            // from here instead of answering false.
            const bool anyID = de::fuzz::options().anyID && status == GPS_NORMAL;
            if (!anyID && !validator.isValidPacketID(status, packetID)) {
                trace.end = "invalid packet order";
                return;
            }

            // The two store-info packets are refused unread.
            if (!de::fuzz::options().noStoreSkip &&
                (packetID == Packet::PACKET_GC_OTHER_STORE_INFO || packetID == Packet::PACKET_GC_MY_STORE_INFO)) {
                trace.end = "store info";
                return;
            }

            // An id with no factory throws InvalidProtocolException from
            // getPacketMaxSize itself.
            if (packetSize > factories.getPacketMaxSize(packetID)) {
                trace.end = "too large packet size";
                return;
            }

            // The body has not all arrived: wait for more.
            if (in.length() < szPacketHeader + packetSize)
                return;

            // The sequence is checked and counted as the frame is
            // consumed, never while it waits for its body.
            if (packetSequence != session.expectedSequence) {
                trace.end = "packet sequence error";
                return;
            }
            session.expectedSequence++;
            session.frames++;
            trace.frames.push_back({packetID, packetSize, 'R'});

            std::unique_ptr<Packet> pPacket(factories.createPacket(packetID));
            de::fuzz::readPacket(in, *pPacket, packetID, packetSize);
        } catch (IgnorePacketException&) {
            // An ignored packet is skipped whole, unread, and its
            // sequence counted once it has been.
            if (packetSize > factories.getPacketMaxSize(packetID)) {
                trace.end = "too large ignored packet size";
                return;
            }
            if (in.length() < szPacketHeader + packetSize)
                return;
            in.skip(szPacketHeader + packetSize);
            if (packetSequence != session.expectedSequence) {
                trace.end = "packet sequence error";
                return;
            }
            session.expectedSequence++;
            session.frames++;
            trace.frames.push_back({packetID, packetSize, 'S'});
        }
    }
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    de::fuzz::initialise();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    de::fuzz::Input input;
    if (!de::fuzz::parse(data, size, input))
        return 0;

    de::fuzz::run([&] {
        de::fuzz::deliverEveryWay(input, data, size, [&](const de::fuzz::Schedule& cuts, de::fuzz::Trace& trace) {
            // GamePlayer reads through a SocketEncryptInputStream whose code
            // comes from the zone the player enters.
            Socket socket(new SocketImpl());
            SocketEncryptInputStream in(&socket, (uint)de::fuzz::streamCapacity(input));
            in.setEncryptCode(input.code);
            Session session;
            de::fuzz::deliver(in, input, cuts, trace,
                              [&](de::fuzz::Trace& t) { receive(in, input.status, session, t); });
        });
    });
    return 0;
}
