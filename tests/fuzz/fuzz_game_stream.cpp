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
//               The mirror carries the switches and the body oracle,
//               which the production gate has no hook for. So that the
//               fuzzing covers the gate itself too, each input the
//               switches leave alone is also delivered byte by byte
//               through GameFrameGate::next, linked from the
//               gameserver's sources, and must read the same packets and
//               end the same way as the mirror's whole delivery: a gate
//               that drifted from the mirror, or that depends on where
//               the bytes were cut, aborts the target.
//
//////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "GameFrameGate.h"
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

// The mirror's name for each of the production gate's refusals.
const char* refusalName(de::GameFrameRefusal refusal) {
    switch (refusal) {
    case de::GameFrameRefusal::IdOutOfRange:
        return "too large packet id";
    case de::GameFrameRefusal::InvalidOrder:
        return "invalid packet order";
    case de::GameFrameRefusal::StoreInfo:
        return "store info";
    case de::GameFrameRefusal::TooLarge:
        return "too large packet size";
    case de::GameFrameRefusal::IgnoredTooLarge:
        return "too large ignored packet size";
    case de::GameFrameRefusal::OutOfSequence:
        return "packet sequence error";
    case de::GameFrameRefusal::None:
        break;
    }
    return "refused with no refusal";
}

// As receive, through the production gate: GamePlayer::processCommand's
// loop, less the handler.
void receiveThroughGate(SocketInputStream& in, PlayerStatus status, de::GameFrameGate& gate, int& frames,
                        de::fuzz::Trace& trace) {
    PacketFactoryManager& factories = de::kernelContext().packetFactories();
    PacketValidator& validator = de::kernelContext().packetValidator();

    while (true) {
        if (frames == de::fuzz::kMaxFrames) {
            trace.end = "frame cap";
            return;
        }

        // The mirror records a frame before reading it, so a body the read
        // refuses is in its trace. The gate throws that refusal from
        // next(), which has then consumed the frame; a refusal from the
        // factory table, before any read, consumes nothing.
        char header[szPacketHeader];
        const bool peeked = in.peek(&header[0], szPacketHeader);
        const uint before = in.length();
        de::GameFrame frame;
        try {
            frame = gate.next(in, status, factories, validator);
        } catch (ProtocolException&) {
            if (peeked && in.length() != before) {
                PacketID_t id;
                PacketSize_t size;
                std::memcpy(&id, &header[0], szPacketID);
                std::memcpy(&size, &header[szPacketID], szPacketSize);
                trace.frames.push_back({id, size, 'R'});
            }
            throw;
        }

        switch (frame.step) {
        case de::GameFrameStep::AwaitHeader:
        case de::GameFrameStep::AwaitBody:
        case de::GameFrameStep::AwaitIgnoredBody:
            return;
        case de::GameFrameStep::Read:
            frames++;
            trace.frames.push_back({frame.id, frame.size, 'R'});
            break;
        case de::GameFrameStep::Skipped:
            frames++;
            trace.frames.push_back({frame.id, frame.size, 'S'});
            break;
        case de::GameFrameStep::Refused:
            trace.end = refusalName(frame.refusal);
            return;
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
        const de::fuzz::Trace whole =
            de::fuzz::deliverEveryWay(input, data, size, [&](const de::fuzz::Schedule& cuts, de::fuzz::Trace& trace) {
                // GamePlayer reads through a SocketEncryptInputStream whose
                // code comes from the zone the player enters.
                Socket socket(new SocketImpl());
                SocketEncryptInputStream in(&socket, (uint)de::fuzz::streamCapacity(input));
                in.setEncryptCode(input.code);
                Session session;
                de::fuzz::deliver(in, input, cuts, trace,
                                  [&](de::fuzz::Trace& t) { receive(in, input.status, session, t); });
            });

        if (de::fuzz::options().anyID || de::fuzz::options().noStoreSkip)
            return;
        Socket socket(new SocketImpl());
        SocketEncryptInputStream in(&socket, (uint)de::fuzz::streamCapacity(input));
        in.setEncryptCode(input.code);
        de::GameFrameGate gate;
        int frames = 0;
        de::fuzz::Trace gated;
        de::fuzz::deliver(in, input, de::fuzz::byteSchedule(input), gated,
                          [&](de::fuzz::Trace& t) { receiveThroughGate(in, input.status, gate, frames, t); });
        de::fuzz::expectSameTrace(whole, gated, "byte by byte through GameFrameGate");
    });
    return 0;
}
