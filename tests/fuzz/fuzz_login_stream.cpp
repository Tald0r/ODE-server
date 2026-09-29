//////////////////////////////////////////////////////////////////////
//
// Filename    : fuzz_login_stream.cpp
// Description : Packet-read fuzz target for the loginserver's client
//               connection. It mirrors LoginPlayer::processCommand
//               (src/server/loginserver/LoginPlayer.cpp) up to the point
//               where the packet has been read, and stops short of the
//               handler: every gate that decides whether a client's
//               bytes reach a packet's read() is here, in production
//               order, and nothing after it.
//
//               Built with __LOGIN_SERVER__ and linked with
//               LoginServerPackets, so the factory table is the
//               loginserver's own: Concat<LoginOnlyFactories,
//               ClientLinkFactories>. Unlike the gameserver, the
//               loginserver checks no sequence byte and reads through a
//               plain SocketInputStream, so the input's code byte is
//               ignored.
//
//               The input format and the switches are described in
//               StreamFuzz.h.
//
//////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "Socket.h"
#include "SocketImpl.h"
#include "StreamFuzz.h"

// tests/fuzz/CMakeLists.txt prefixes the golden seeds with these status
// bytes, so a renumbering must move the seeds with it.
static_assert(LPS_BEGIN_SESSION == 1, "the login seed corpus is written for LPS_BEGIN_SESSION == 1");
static_assert(LPS_PC_MANAGEMENT == 4, "the login seed corpus is written for LPS_PC_MANAGEMENT == 4");

namespace {

void receive(SocketInputStream& in, PlayerStatus status) {
    // :140 - while a kick is being verified nothing is read at all.
    if (status == LPS_WAITING_FOR_GL_KICK_VERIFY)
        return;

    PacketFactoryManager& factories = de::kernelContext().packetFactories();
    PacketValidator& validator = de::kernelContext().packetValidator();

    for (int frame = 0; frame < de::fuzz::kMaxFrames; frame++) {
        char header[szPacketHeader];
        // :172 - fewer than seven bytes buffered: wait for more.
        if (!in.peek(header, szPacketHeader))
            return;

        PacketID_t packetID;
        PacketSize_t packetSize;
        std::memcpy(&packetID, &header[0], szPacketID);
        std::memcpy(&packetSize, &header[szPacketID], szPacketSize);

        // :188 - the debug line names the packet before any check, and
        // getPacketName throws InvalidProtocolException for an id with no
        // factory, so this is the first gate in practice.
        (void)factories.getPacketName(packetID);

        // :193 - "too large packet id".
        if (packetID >= Packet::PACKET_MAX)
            return;

        try {
            // :199 - "invalid packet order". A PIST_IGNORE_EXCEPT status
            // throws IgnorePacketException from here instead.
            if (!validator.isValidPacketID(status, packetID))
                return;

            // :206 - "too large packet size".
            if (packetSize > factories.getPacketMaxSize(packetID))
                return;

            // :211 - the body has not all arrived: wait for more.
            if (in.length() < szPacketHeader + packetSize)
                return;

            // :225 and :230.
            std::unique_ptr<Packet> pPacket(factories.createPacket(packetID));
            de::fuzz::readPacket(in, *pPacket, packetID, packetSize);
        } catch (IgnorePacketException&) {
            // :253-268 - an ignored packet is skipped whole, unread.
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

    // LoginPlayer reads through a plain SocketInputStream (LoginPlayer.cpp:62).
    Socket socket(new SocketImpl());
    SocketInputStream in(&socket, (uint)de::fuzz::streamCapacity(input));
    de::fuzz::loadStream(in, input);

    de::fuzz::run([&] { receive(in, input.status); });
    return 0;
}
