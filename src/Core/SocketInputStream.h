//////////////////////////////////////////////////////////////////////
//
// Filename    : SocketInputStream.h
// Written by  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////
//
// *Reiot's Notes*
//
// One of the classes used most heavily in the system.
// Speed matters a great deal here, so if the speed has to be pushed
// further, re-write it without exceptions.
//
// A nonblocking socket is said to raise an exception once in every
// thousand or so, and the overhead of wrapping that in a
// NonBlockingIOException is reckoned to be negligible.
//
//////////////////////////////////////////////////////////////////////

#ifndef __SOCKET_INPUT_STREAM_H__
#define __SOCKET_INPUT_STREAM_H__

// include files
#include <cstddef>
#include <cstring>
#include <span>

#include "Assert.h"
#include "Exception.h"
#include "Socket.h"
#include "Types.h"
#include "WireTypes.h"

// constant definitions
const uint DefaultSocketInputBufferSize = 81920;

// forward declaration
class Packet;

//////////////////////////////////////////////////////////////////////
//
// class SocketInputStream
//
//////////////////////////////////////////////////////////////////////

class SocketInputStream {
    // The unit tests and the packet-read fuzz targets load the buffer
    // directly through this class (tests/support/SocketInputStreamTestAccess.h),
    // as if fill() had received the bytes, so a packet can be read from
    // memory without a socket.
    friend class SocketInputStreamTestAccess;

    //////////////////////////////////////////////////
    // constructor/destructor
    //////////////////////////////////////////////////
public:
    // constructor
    SocketInputStream(Socket* sock, uint BufferSize = DefaultSocketInputBufferSize);

    // destructor
    virtual ~SocketInputStream() noexcept;


    //////////////////////////////////////////////////
    // methods
    //////////////////////////////////////////////////
public:
    // read data from stream (input buffer)
    //
    // The span overload carries the implementation: it binds the buffer
    // to its length instead of trusting the caller to pass a matching
    // pair. The char*/uint form is kept verbatim for the existing call
    // sites and forwards, so bounds behaviour and the exceptions thrown
    // (InvalidProtocolException on an empty request,
    // InsufficientDataException when the buffer holds less) are the same
    // through either entry point.
    uint read(std::span<std::byte> dst);
    uint read(char* buf, uint len);
    uint read(string& str, uint len);
    void readPacket(Packet* p);

    // Raw scalar read: copies sizeof(T) bytes of the object
    // representation straight off the wire. Only de::WireScalar types
    // may do that -- see WireTypes.h for what that admits and why.
    template <de::WireScalar T> uint read(T& buf);

    // Deliberately an overload rather than only a constraint on the one
    // above: it keeps a rejected type from quietly finding some other
    // viable overload, and reports the type by name instead of "no
    // matching member function".
    template <typename T> uint read(T& buf) {
        static_assert(de::WireScalar<T>,
                      "SocketInputStream::read<T>: T is not a protocol wire scalar. Reading it would copy "
                      "sizeof(T) raw bytes of a pointer, an object layout or a platform-sized integer off the "
                      "wire. Read the protocol's own fixed-width fields (see WireTypes.h), or read a length "
                      "prefix plus read(char*, uint) / read(std::span<std::byte>) for a buffer.");
        return 0;
    }

    // peek data from stream (input buffer). Same split as read() above.
    bool peek(std::span<std::byte> dst);
    bool peek(char* buf, uint len);

    // skip data from stream (input buffer)
    void skip(uint len);

    // fill stream (input buffer) from socket
    uint fill();
    uint fill_RAW();

    // resize buffer
    void resize(int size);

    // get buffer length
    uint capacity() const {
        return m_BufferLen;
    }

    // get data length in buffer
    uint length() const;
    uint size() const {
        return length();
    }

    // check if buffer is empty
    bool isEmpty() const {
        return m_Head == m_Tail;
    }

    // get debug string
    string toString() const;


    //////////////////////////////////////////////////
    // attributes
    //////////////////////////////////////////////////
private:
    // socket
    Socket* m_pSocket;

    // buffer
    char* m_Buffer;

    // buffer length
    uint m_BufferLen;

    // buffer head/tail
    uint m_Head;
    uint m_Tail;
    // add by viva 2008-12-31
public:
    WORD m_EncryptKey;
    BYTE* m_HashTable;
    void setKey(WORD EncryptKey, BYTE* HashTable) {
        m_EncryptKey = EncryptKey;
        m_HashTable = HashTable;
    }
    // The transform applied to the bytes as they arrive, and the seam it
    // is exercised through. It undoes what the sending stream's
    // EncryptData did, run for run: fill() applies it to each chunk the
    // socket hands over, in arrival order, exactly once.
    virtual WORD EncryptData(WORD EncryptKey, char* buf, int len);
    // end
};


//////////////////////////////////////////////////////////////////////
//
// read data from input buffer
//
// The one copy of the ring-buffer walk: the char*/uint overload and the
// scalar read<T>() template both come through here. The span binds the
// destination to its own length, so the two can no longer disagree.
//
// Defined inline, in the header, ON PURPOSE. read<T>() calls this with
// dst.size() == sizeof(T); out of line that becomes a call with a runtime
// length, and the memcpy below stops folding to the single load that
// docs/TOOLCHAIN.md section 2 relies on. Every scalar field of every
// packet goes through here, so keep the definition visible.
//
//////////////////////////////////////////////////////////////////////
inline uint SocketInputStream::read(std::span<std::byte> dst) {
    char* buf = reinterpret_cast<char*>(dst.data());
    const uint len = (uint)dst.size();

    Assert(buf != NULL);

    if (len == 0)
        throw InvalidProtocolException("len==0");

    // Less data buffered than the caller asked for. (The original
    // Korean note here did not survive the encoding migration.)
    if (len > length())
        throw InsufficientDataException(len - length());

    if (m_Head < m_Tail) { // normal order

        //
        //    H   T
        // 0123456789
        // ...abcd...
        //

        memcpy(buf, &m_Buffer[m_Head], len);
    } else { // reversed order ( m_Head > m_Tail )

        //
        //     T  H
        // 0123456789
        // abcd...efg
        //

        uint rightLen = m_BufferLen - m_Head;
        if (len <= rightLen) {
            memcpy(buf, &m_Buffer[m_Head], len);
        } else {
            memcpy(buf, &m_Buffer[m_Head], rightLen);
            memcpy(&buf[rightLen], m_Buffer, len - rightLen);
        }
    }

    m_Head = (m_Head + len) % m_BufferLen;

    return len;
}


//////////////////////////////////////////////////////////////////////
//
// read a scalar from the input buffer
//
// The ring-buffer walk lives in the read(std::span<std::byte>)
// definition just above -- inline, so sizeof(T) still reaches its
// memcpy as a constant. sizeof(T) is never 0 and &buf is never null, so
// neither guard in the span overload can fire here.
//
// reinterpret_cast to std::byte* is not an aliasing violation: std::byte
// (like char) may alias any object. std::bit_cast would need a second
// copy through a temporary array to say the same thing, so it is not
// used here.
//
//////////////////////////////////////////////////////////////////////
template <de::WireScalar T> uint SocketInputStream::read(T& buf) {
    return read(std::span<std::byte>(reinterpret_cast<std::byte*>(&buf), sizeof(T)));
}

#endif
