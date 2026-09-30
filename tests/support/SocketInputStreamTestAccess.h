//////////////////////////////////////////////////////////////////////
//
// Filename    : SocketInputStreamTestAccess.h
// Description : Loads bytes straight into a SocketInputStream's buffer,
//               as if fill() had received them, so a packet can be read
//               from memory instead of from a connected socket.
//               SocketInputStream befriends this class for that purpose
//               alone.
//
//               It uses no test framework: the gtest suites and the
//               packet-read fuzz targets (tests/fuzz/), which must not
//               link gtest, both include it.
//
//               The stream still needs a Socket object: its constructor
//               asserts one. Socket(new SocketImpl()) is one that owns no
//               descriptor, so building it makes no system call and
//               destroying it closes nothing:
//
//                   Socket socket(new SocketImpl());
//                   SocketEncryptInputStream in(&socket, 1024);
//                   SocketInputStreamTestAccess::Preload(in, bytes, n);
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
    // The bytes then go through the same receive transform fill() applies
    // to each chunk, EncryptData(), which carries the stream's key on from
    // the value it holds. Today that transform returns at once and
    // changes nothing, so the bytes are read exactly as given; should it
    // ever do work again, a preloaded input is still treated as bytes
    // straight off the socket. A SocketEncryptInputStream undoes its
    // per-field encryption as the packet is read, as it always does.
    static bool Preload(SocketInputStream& stream, const unsigned char* data, std::size_t len) {
        if (len >= stream.m_BufferLen)
            return false;
        if (len > 0)
            std::memcpy(stream.m_Buffer, data, len);
        stream.m_Head = 0;
        stream.m_Tail = static_cast<uint>(len);
        stream.m_EncryptKey = stream.EncryptData(stream.m_EncryptKey, stream.m_Buffer, static_cast<int>(len));
        return true;
    }

    // As Preload(), but the bytes start at offset `head` of the ring buffer
    // and continue from its front once they reach the end, so the read
    // takes the wrap-around branches. At most capacity() - 1 bytes fit,
    // and `head` must lie inside the buffer; otherwise nothing is loaded
    // and false is returned.
    static bool PreloadAt(SocketInputStream& stream, std::size_t head, const unsigned char* data, std::size_t len) {
        if (len >= stream.m_BufferLen || head >= stream.m_BufferLen)
            return false;
        for (std::size_t i = 0; i < len; i++)
            stream.m_Buffer[(head + i) % stream.m_BufferLen] = static_cast<char>(data[i]);
        stream.m_Head = static_cast<uint>(head);
        stream.m_Tail = static_cast<uint>((head + len) % stream.m_BufferLen);
        return true;
    }
};

#endif
