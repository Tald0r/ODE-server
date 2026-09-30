//////////////////////////////////////////////////////////////////////
//
// Filename    : StreamFuzz.h
// Description : What the two packet-read fuzz targets share: the input
//               format, the environment switches, the packet tables,
//               the in-memory stream, the body oracle and the exception
//               policy. Each target (fuzz_game_stream.cpp,
//               fuzz_login_stream.cpp) adds the loop that mirrors its
//               server's receive gate; the game target also runs the
//               production gate, GameFrameGate, against that mirror.
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
//               Each input is delivered three times, to a fresh session
//               each time, the way TCP may hand the same bytes over: whole,
//               in one receive; one byte per receive, so every frame is
//               seen cut at every point; and in chunks of 1 to 48 bytes
//               whose lengths come from a hash of the input, so a receive
//               can end one frame, hold several whole ones and start the
//               next. The receive loop runs after every receive. All three
//               must read the same packets in the same order and end the
//               same way (the same refusal, or waiting on the same bytes),
//               or the target aborts, naming the schedule and the first
//               difference: a receive loop must not depend on where the
//               network cut its bytes. The game target, unless
//               DE_FUZZ_ANY_ID or DE_FUZZ_NO_STORE_SKIP is on (the
//               production gate has neither), also delivers the input
//               byte by byte through GameFrameGate::next itself and
//               aborts unless that too ends as the whole delivery did.
//
//               Any exception that is not a ProtocolException aborts the
//               target: the player managers catch only ProtocolException,
//               so anything else a read() throws leaves the receive loop
//               and, on the gameserver, stops the zone thread and the
//               server with it.
//
//               The body oracle aborts too: readPacket() must leave the
//               stream exactly the 7 + size bytes its header declared
//               further on, whether it returns or refuses the body with
//               a ProtocolException, so the next frame is read from its
//               first byte.
//
//               Environment switches, read once at start-up:
//
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
//                                         GameFrameGate::next refuses
//                                         unread. The validator
//                                         refuses them first unless
//                                         DE_FUZZ_ANY_ID is on too.
//
//               Not covered: the input is delivered from the start of the
//               stream's buffer, which is sized to hold all of it, so the
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
#include <string>
#include <vector>

#include "Exception.h"
#include "KernelContext.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PlayerStatus.h"
#include "SocketInputStream.h"
#include "SocketInputStreamTestAccess.h"

namespace de::fuzz {

// Frames a session consumes, read or skipped, at most; the delivery ends
// there and the rest is ignored.
inline constexpr int kMaxFrames = 64;

// Inputs longer than this are ignored rather than read, which keeps a run
// from spending its time on megabytes of padding.
inline constexpr std::size_t kMaxInput = 64 * 1024;

// GamePlayer and LoginPlayer both start with a 1024-byte buffer
// (defaultGamePlayerInputStreamSize, defaultLoginPlayerInputStreamSize);
// fill() grows it on demand, so the target sizes its stream to hold the
// whole input instead.
inline constexpr std::size_t kSessionBuffer = 1024;

struct Options {
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


// Aborts unless readPacket() moved the stream exactly one frame on.
inline void expectOneFrameConsumed(uint before, uint after, PacketID_t id, PacketSize_t size, const char* outcome) {
    const uint consumed = before - after;
    if (consumed != szPacketHeader + size) {
        std::fprintf(stderr, "fuzz harness: packet %u declared %u body bytes, and readPacket %s after consuming %d\n",
                     (unsigned)id, (unsigned)size, outcome, (int)consumed - (int)szPacketHeader);
        std::abort();
    }
}

// SocketInputStream::readPacket(), as the receive loops call it, with the
// body oracle around it. The loops call it only once the whole frame is
// buffered, so it consumes exactly that frame, or the next one is parsed
// from inside this one.
inline void readPacket(SocketInputStream& stream, Packet& packet, PacketID_t id, PacketSize_t size) {
    const uint before = stream.length();
    try {
        stream.readPacket(&packet);
    } catch (ProtocolException&) {
        expectOneFrameConsumed(before, stream.length(), id, size, "refused the body");
        throw;
    }
    expectOneFrameConsumed(before, stream.length(), id, size, "returned");
}

// What a receive loop did with one delivery of an input: the frames it
// consumed, in order, and how the session ended. A session that ran out of
// input while waiting for more has an empty `end` and `left` bytes still
// buffered.
struct Trace {
    struct Frame {
        PacketID_t id;
        PacketSize_t size;
        // 'R' for a frame handed to readPacket, 'S' for one skipped unread.
        char how;
        bool operator==(const Frame& o) const {
            return id == o.id && size == o.size && how == o.how;
        }
    };
    std::vector<Frame> frames;
    std::string end;
    uint left = 0;

    bool ended() const {
        return !end.empty();
    }
};

// Where each receive of a delivery ends, as offsets into the input's
// stream bytes: ascending, the last one the stream's length.
typedef std::vector<std::size_t> Schedule;

inline Schedule wholeSchedule(const Input& input) {
    return Schedule{input.length};
}

inline Schedule byteSchedule(const Input& input) {
    Schedule cuts;
    for (std::size_t at = 1; at <= input.length; at++)
        cuts.push_back(at);
    return cuts;
}

// Chunks of 1 to 48 bytes, their lengths drawn from a generator seeded
// with a hash of the whole input, so the fuzzer varies the cuts by
// varying the bytes, and a replay cuts a saved input where the run did.
inline Schedule chunkSchedule(const Input& input, const std::uint8_t* data, std::size_t size) {
    std::uint64_t state = 1469598103934665603ULL;
    for (std::size_t i = 0; i < size; i++)
        state = (state ^ data[i]) * 1099511628211ULL;
    Schedule cuts;
    std::size_t at = 0;
    while (at < input.length) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        at += 1 + (std::size_t)((state >> 33) % 48);
        if (at > input.length)
            at = input.length;
        cuts.push_back(at);
    }
    return cuts;
}

// Hands the input's stream bytes to `stream` one receive per cut, as
// fill() would, and runs `receive` after each until the trace ends. A
// ProtocolException is the server dropping the connection, so it ends the
// trace; anything else is passed on to run(). `receive` is the target's
// loop over what is buffered; it keeps its own session state between
// calls and returns when it waits for more bytes or ends the trace.
inline void deliver(SocketInputStream& stream, const Input& input, const Schedule& cuts, Trace& trace,
                    const std::function<void(Trace&)>& receive) {
    std::size_t at = 0;
    for (std::size_t cut : cuts) {
        if (!SocketInputStreamTestAccess::Append(stream, input.bytes + at, cut - at)) {
            std::fprintf(stderr, "fuzz harness: the stream refused %zu bytes at offset %zu\n", cut - at, at);
            std::abort();
        }
        at = cut;
        try {
            receive(trace);
        } catch (ProtocolException& e) {
            trace.end = e.getName() + ": " + e.getMessage();
        }
        if (trace.ended())
            break;
    }
    trace.left = stream.length();
}

// Aborts unless `other` read what `whole` read and ended as it did.
inline void expectSameTrace(const Trace& whole, const Trace& other, const char* schedule) {
    const std::size_t n = whole.frames.size() < other.frames.size() ? whole.frames.size() : other.frames.size();
    for (std::size_t i = 0; i < n; i++) {
        if (!(whole.frames[i] == other.frames[i])) {
            std::fprintf(stderr,
                         "fuzz harness: delivered %s, frame %zu was packet %u (%u bytes, %c); whole, packet %u "
                         "(%u bytes, %c)\n",
                         schedule, i, (unsigned)other.frames[i].id, (unsigned)other.frames[i].size, other.frames[i].how,
                         (unsigned)whole.frames[i].id, (unsigned)whole.frames[i].size, whole.frames[i].how);
            std::abort();
        }
    }
    if (whole.frames.size() != other.frames.size()) {
        std::fprintf(stderr, "fuzz harness: delivered %s, %zu frames were consumed; whole, %zu\n", schedule,
                     other.frames.size(), whole.frames.size());
        std::abort();
    }
    if (whole.end != other.end) {
        std::fprintf(stderr, "fuzz harness: delivered %s, the session ended with \"%s\"; whole, with \"%s\"\n",
                     schedule, other.end.c_str(), whole.end.c_str());
        std::abort();
    }
    if (!whole.ended() && whole.left != other.left) {
        std::fprintf(stderr, "fuzz harness: delivered %s, %u bytes were left waiting; whole, %u\n", schedule,
                     other.left, whole.left);
        std::abort();
    }
}

// Delivers the input whole, byte by byte and in hashed chunks, each to a
// fresh session from `session`, and aborts unless the three agree.
// `session` builds a stream and a receive loop over it and delivers the
// input to them by the schedule it is given. Returns the whole delivery's
// trace, for a target that checks another loop against it.
inline Trace deliverEveryWay(const Input& input, const std::uint8_t* data, std::size_t size,
                             const std::function<void(const Schedule&, Trace&)>& session) {
    Trace whole;
    session(wholeSchedule(input), whole);
    Trace bytes;
    session(byteSchedule(input), bytes);
    expectSameTrace(whole, bytes, "byte by byte");
    Trace chunks;
    session(chunkSchedule(input, data, size), chunks);
    expectSameTrace(whole, chunks, "in hashed chunks");
    return whole;
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
