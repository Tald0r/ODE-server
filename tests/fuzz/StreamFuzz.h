//////////////////////////////////////////////////////////////////////
//
// Filename    : StreamFuzz.h
// Description : What the two packet-read fuzz targets share: the input
//               format, the environment switches, the packet tables,
//               the in-memory stream, the body oracle and the exception
//               policy. Each target (fuzz_game_stream.cpp,
//               fuzz_login_stream.cpp) adds only the loop that mirrors
//               its server's processCommand.
//
//               An input is
//
//                   [code byte][status byte][raw stream bytes]
//
//               the encrypt code the session's input stream decodes with
//               (the loginserver's plain stream ignores it), the player
//               status the validator is asked about (taken modulo
//               PLAYER_STATUS_MAX), and the bytes a client could have
//               sent: any number of 7-byte headers and bodies, read at
//               most kMaxFrames packets deep.
//
//               An input shorter than the two header bytes or longer
//               than kMaxInput is not run; the replay driver refuses one
//               instead of passing it.
//
//               Any exception that is not a ProtocolException aborts the
//               target: the player managers catch only ProtocolException,
//               so anything else a read() throws leaves the receive loop
//               and, on the gameserver, stops the zone thread and the
//               server with it.
//
//               Environment switches, read once at start-up:
//
//                 DE_FUZZ_STRICT_BODY=1   abort when a read() consumes
//                                         anything but the 7 + size
//                                         bytes its header declared.
//                                         readPacket() is not bounded by
//                                         the frame, so a short or long
//                                         read desynchronises every
//                                         packet after it.
//                 DE_FUZZ_ALLOW_EXCEPTIONS=1
//                                         end an input quietly on any
//                                         exception instead, to look past
//                                         that class of finding.
//                 DE_FUZZ_ANY_ID=1        (game only) admit every
//                                         registered id in GPS_NORMAL,
//                                         as the validator did before it
//                                         was narrowed to what a client
//                                         sends, so the reads behind the
//                                         gate stay fuzzed.
//                 DE_FUZZ_NO_STORE_SKIP=1 (game only) read the two
//                                         store-info packets that
//                                         GamePlayer::processCommand
//                                         refuses unread. The validator
//                                         refuses them first unless
//                                         DE_FUZZ_ANY_ID is on too.
//
//               Not covered: the stream is loaded with the input at the
//               start of its buffer and sized to hold all of it, so the
//               ring buffer's wrap-around branches in read(), peek() and
//               skip() and fill()'s growth never run, although a client
//               decides where its bytes land in the buffer.
//
//               Assert appends to assertion_failed.log in the working
//               directory, so run the targets from a scratch directory.
//               libFuzzer runs want -close_fd_mask=3: every packet read
//               prints its toString() to cout, and although the targets
//               silence cout, stderr stays noisy.
//
//////////////////////////////////////////////////////////////////////

#ifndef __STREAM_FUZZ_H__
#define __STREAM_FUZZ_H__

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>

#include "Exception.h"
#include "KernelContext.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PlayerStatus.h"
#include "SocketInputStream.h"
#include "SocketInputStreamTestAccess.h"

namespace de::fuzz {

// Packets read from one input at most; the rest is ignored.
inline constexpr int kMaxFrames = 64;

// Inputs longer than this are ignored rather than read, which keeps a run
// from spending its time on megabytes of padding.
inline constexpr std::size_t kMaxInput = 64 * 1024;

// GamePlayer and LoginPlayer both start with a 1024-byte buffer
// (GamePlayer.cpp:60, LoginPlayer.cpp:34); fill() grows it on demand, so
// the target sizes its stream to hold the whole input instead.
inline constexpr std::size_t kSessionBuffer = 1024;

struct Options {
    bool strictBody = false;
    bool strictExceptions = true;
    bool noStoreSkip = false;
    bool anyID = false;
};

inline Options& options() {
    static Options o;
    return o;
}

inline bool envIsOne(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && std::strcmp(v, "1") == 0;
}

// Builds the server's factory table and validator the way its main()
// does, registers them where the receive loops look them up, and turns
// cout off: SocketInputStream::readPacket() prints every packet's
// toString(), which still runs, but the text goes nowhere.
inline void initialise() {
    options().strictBody = envIsOne("DE_FUZZ_STRICT_BODY");
    options().strictExceptions = !envIsOne("DE_FUZZ_ALLOW_EXCEPTIONS");
    options().noStoreSkip = envIsOne("DE_FUZZ_NO_STORE_SKIP");
    options().anyID = envIsOne("DE_FUZZ_ANY_ID");

    std::cout.setstate(std::ios_base::badbit);

    PacketFactoryManager* pFactories = new PacketFactoryManager();
    pFactories->init();
    PacketValidator* pValidator = new PacketValidator();
    pValidator->init();
    de::kernelContext().setPacketFactoryManager(pFactories);
    de::kernelContext().setPacketValidator(pValidator);
}

struct Input {
    uchar code = 0;
    PlayerStatus status = PlayerStatus(0);
    const unsigned char* bytes = nullptr;
    std::size_t length = 0;
};

// Whether an input of this length is run at all.
inline bool runsInput(std::size_t size) {
    return size >= 2 && size <= kMaxInput;
}

inline bool parse(const std::uint8_t* data, std::size_t size, Input& input) {
    if (!runsInput(size))
        return false;
    input.code = data[0];
    input.status = PlayerStatus(data[1] % PLAYER_STATUS_MAX);
    input.bytes = data + 2;
    input.length = size - 2;
    return true;
}

inline std::size_t streamCapacity(const Input& input) {
    return input.length + 1 > kSessionBuffer ? input.length + 1 : kSessionBuffer;
}

inline void loadStream(SocketInputStream& stream, const Input& input) {
    if (!SocketInputStreamTestAccess::Preload(stream, input.bytes, input.length)) {
        std::fprintf(stderr, "fuzz harness: the stream refused a %zu-byte input\n", input.length);
        std::abort();
    }
}

// SocketInputStream::readPacket(), as both receive loops call it, with
// the body oracle around it.
inline void readPacket(SocketInputStream& stream, Packet& packet, PacketID_t id, PacketSize_t size) {
    const uint before = stream.length();
    stream.readPacket(&packet);
    const uint consumed = before - stream.length();
    if (options().strictBody && consumed != szPacketHeader + size) {
        std::fprintf(stderr, "fuzz harness: packet %u declared %u body bytes, read() consumed %u\n", (unsigned)id,
                     (unsigned)size, consumed - szPacketHeader);
        std::abort();
    }
}

// Runs one input's receive loop. A ProtocolException is how a server
// turns a malformed packet away, so it always ends the input quietly.
// Anything else is a crash, unless DE_FUZZ_ALLOW_EXCEPTIONS lets it end
// the input quietly too.
inline void run(const std::function<void()>& receive) {
    try {
        receive();
    } catch (ProtocolException&) {
    } catch (Throwable& t) {
        if (options().strictExceptions) {
            std::fprintf(stderr, "fuzz harness: non-protocol exception: %s\n", t.toString().c_str());
            std::abort();
        }
    } catch (std::exception& e) {
        if (options().strictExceptions) {
            std::fprintf(stderr, "fuzz harness: std::exception: %s\n", e.what());
            std::abort();
        }
    }
}

} // namespace de::fuzz

#endif
