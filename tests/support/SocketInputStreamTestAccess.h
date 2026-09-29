//////////////////////////////////////////////////////////////////////
//
// Filename    : SocketInputStreamTestAccess.h
// Description : Loads bytes straight into a SocketInputStream's buffer,
//               as if fill() had received them, so a packet can be read
//               from memory with no socket. SocketInputStream befriends
//               this class for that purpose alone.
//
//               It uses no test framework: the gtest suites and the fuzz
//               targets (tests/fuzz/), which must not link gtest, both
//               include it.
//
//////////////////////////////////////////////////////////////////////

#ifndef __SOCKET_INPUT_STREAM_TEST_ACCESS_H__
#define __SOCKET_INPUT_STREAM_TEST_ACCESS_H__

#include <cstddef>
#include <cstring>

#include "SocketInputStream.h"

class SocketInputStreamTestAccess {
public:
    // Replaces whatever the stream holds with data[0, len), starting at
    // the front of the buffer, so the bytes are contiguous and the read
    // never takes the ring buffer's wrap-around branch.
    //
    // fill() keeps one slot free so a full buffer is never mistaken for an
    // empty one, so at most capacity() - 1 bytes fit; a longer len loads
    // nothing and returns false.
    //
    // The bytes go in as given: fill() runs each received chunk through
    // EncryptData() first, and Preload() does not. A SocketEncryptInputStream
    // still undoes its per-field encryption as the packet is read.
    static bool Preload(SocketInputStream& stream, const unsigned char* data, std::size_t len) {
        if (len >= stream.m_BufferLen)
            return false;
        if (len > 0)
            std::memcpy(stream.m_Buffer, data, len);
        stream.m_Head = 0;
        stream.m_Tail = static_cast<uint>(len);
        return true;
    }
};

#endif
