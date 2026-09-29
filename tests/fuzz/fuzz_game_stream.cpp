//////////////////////////////////////////////////////////////////////
//
// Filename    : fuzz_game_stream.cpp
// Description : Packet-read fuzz target for the gameserver's client
//               connection. It mirrors GamePlayer::processCommand
//               (src/server/gameserver/GamePlayer.cpp) up to the point
//               where the packet has been read, and stops short of the
//               handler: every gate that decides whether a client's
//               bytes reach a packet's read() is here, in production
//               order, and nothing after it.
//
//               Built with __GAME_SERVER__ and linked with
//               GameServerPackets, so the factory table is the
//               gameserver's own: Concat<GameOnlyFactories,
//               ClientLinkFactories, GuildLinkFactories>. In GPS_NORMAL
//               the validator admits any id, so a client can make the
//               gameserver read every packet in that table, including
//               the GC, GG, GL and LG ones it only ever sends.
//
//               The input format and the switches are described in
//               StreamFuzz.h. The loop runs the zone thread's call
//               (Option == true), so the main-thread stop after CGReady
//               and the event heartbeat do not apply.
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

void receive(SocketInputStream& in, PlayerStatus status) {
    PacketFactoryManager& factories = de::kernelContext().packetFactories();
    PacketValidator& validator = de::kernelContext().packetValidator();

    // GamePlayer's m_Sequence starts at 0 (GamePlayer.cpp:78).
    SequenceSize_t expectedSequence = 0;

    for (int frame = 0; frame < de::fuzz::kMaxFrames; frame++) {
        char header[szPacketHeader];
        // :314 - fewer than seven bytes buffered: wait for more.
        if (!in.peek(&header[0], szPacketHeader))
            return;

        PacketID_t packetID;
        PacketSize_t packetSize;
        SequenceSize_t packetSequence;
        std::memcpy(&packetID, &header[0], szPacketID);
        std::memcpy(&packetSize, &header[szPacketID], szPacketSize);
        std::memcpy(&packetSequence, &header[szPacketID + szPacketSize], szSequenceSize);

        // :336 - a sequence byte out of step disconnects; :342 counts it.
        if (packetSequence != expectedSequence)
            return;
        expectedSequence++;

        // :345 - "too large packet id".
        if (packetID >= (int)Packet::PACKET_MAX)
            return;

        try {
            // :356 - "invalid packet order". A PIST_IGNORE_EXCEPT status
            // throws IgnorePacketException from here instead.
            if (!validator.isValidPacketID(status, packetID))
                return;

            // :364 - the two store-info packets are refused unread.
            if (!de::fuzz::options().noStoreSkip &&
                (packetID == Packet::PACKET_GC_OTHER_STORE_INFO || packetID == Packet::PACKET_GC_MY_STORE_INFO))
                return;

            // :372 - "too large packet size"; an id with no factory throws
            // InvalidProtocolException from getPacketMaxSize itself.
            if (packetSize > factories.getPacketMaxSize(packetID))
                return;

            // :381 - the body has not all arrived: wait for more.
            if (in.length() < szPacketHeader + packetSize)
                return;

            // :394 and :399.
            std::unique_ptr<Packet> pPacket(factories.createPacket(packetID));
            de::fuzz::readPacket(in, *pPacket, packetID, packetSize);
        } catch (IgnorePacketException&) {
            // :462-482 - an ignored packet is skipped whole, unread.
            if (packetSize > factories.getPacketMaxSize(packetID))
                return;
            if (in.length() < szPacketHeader + packetSize)
                return;
            in.skip(szPacketHeader + packetSize);
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

    // GamePlayer reads through a SocketEncryptInputStream (GamePlayer.cpp:86)
    // whose code comes from the zone the player enters.
    Socket socket(new SocketImpl());
    SocketEncryptInputStream in(&socket, (uint)de::fuzz::streamCapacity(input));
    in.setEncryptCode(input.code);
    de::fuzz::loadStream(in, input);

    de::fuzz::run([&] { receive(in, input.status); });
    return 0;
}
