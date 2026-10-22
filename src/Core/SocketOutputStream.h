//////////////////////////////////////////////////////////////////////
//
// SocketOutputStream.h
//
// by Reiot
//
//////////////////////////////////////////////////////////////////////

#ifndef __SOCKET_OUTPUT_STREAM_H__
#define __SOCKET_OUTPUT_STREAM_H__

// include files
#include <cstddef>
#include <cstring>
#include <span>

#include "Exception.h"
#include "Socket.h"
#include "Types.h"
#include "WireTypes.h"

// constant definitions
const unsigned int DefaultSocketOutputBufferSize = 81920;

// forward declaration
class Packet;

//////////////////////////////////////////////////////////////////////
//
// class SocketOutputStream
//
//////////////////////////////////////////////////////////////////////

class SocketOutputStream {
    //////////////////////////////////////////////////
    // constructor/destructor
    //////////////////////////////////////////////////
public:
    // constructor
    SocketOutputStream(Socket* sock, uint BufferSize = DefaultSocketOutputBufferSize);

    // destructor
    virtual ~SocketOutputStream() noexcept;


    //////////////////////////////////////////////////
    // methods
    //////////////////////////////////////////////////
public:
    // write data to stream (output buffer)
    //
    // *CAUTION* (translated from the original Korean note)
    // Writing a string to the buffer could prefix its size
    // automatically, but whether that size should be a BYTE or a WORD
    // is an open question. Packets are kept as small as possible, so
    // the caller writes the string size as a BYTE or a WORD by hand,
    // whichever the packet needs.
    //
    // The span overload carries the implementation: it binds the buffer
    // to its length instead of trusting the caller to pass a matching
    // pair. The const char*/uint form is kept verbatim for the existing
    // call sites and forwards to it, so the resize behaviour and the
    // bytes produced are the same through either entry point.
    uint write(std::span<const std::byte> src);
    uint write(const char* buf, uint len);
    uint write(const string& buf) {
        return write(buf.c_str(), buf.size());
    }

    // Write one packet framed as the peer reads it: id, size, sequence
    // byte, body. All or nothing: if the body (or anything else) throws,
    // the stream and the sequence counter are rolled back to where they
    // stood before the call, and the exception propagates.
    void writePacket(const Packet* pPacket);

    // Reserve `len` bytes at the end of the buffer and return a handle
    // for filling them in later, once the bytes that follow are known.
    // The reserved bytes are zero until patched.
    //
    // The handle is the field's distance from the buffer head, not a
    // pointer and not a buffer index. resize() moves the buffered bytes
    // to the front of a new allocation but keeps their order and their
    // distance from the head, so a handle survives a buffer growth
    // caused by the writes made in between -- which is the case that
    // matters, since a packet whose body is larger than the free space
    // grows the buffer while it is being written.
    uint reserveField(uint len);

    // Overwrite a field reserved by reserveField(). The field may
    // straddle the ring buffer's wrap point, so the bytes are placed one
    // at a time instead of by a single copy.
    void patchField(uint slot, std::span<const std::byte> src);

    // Same, for a protocol scalar -- de::WireScalar for the same reason
    // write<T>() is constrained by it: this copies sizeof(T) bytes of the
    // object representation onto the wire.
    template <de::WireScalar T> void patchField(uint slot, T value) {
        patchField(slot, std::span<const std::byte>(reinterpret_cast<const std::byte*>(&value), sizeof(T)));
    }

    // Raw scalar write: copies sizeof(T) bytes of the object
    // representation straight onto the wire. Only de::WireScalar types
    // may do that -- see WireTypes.h for what that admits and why.
    template <de::WireScalar T> uint write(T buf);

    // Deliberately an overload rather than only a constraint on the one
    // above. Without it, constraining write<T> away would let a
    // `write(someCharPointer)` fall back to write(const string&) through
    // a user-defined conversion and silently put a *different* byte
    // sequence on the wire. This keeps it a compile error, named.
    template <typename T> uint write(T buf) {
        static_assert(de::WireScalar<T>,
                      "SocketOutputStream::write<T>: T is not a protocol wire scalar. Writing it would put "
                      "sizeof(T) raw bytes of a pointer, an object layout or a platform-sized integer on the "
                      "wire. Write the protocol's own fixed-width fields (see WireTypes.h), or write a length "
                      "prefix plus write(const char*, uint) / write(std::span<const std::byte>) for a buffer.");
        return 0;
    }
    // flush stream (output buffer) to socket
    uint flush();

    // resize buffer
    void resize(int size);

    // get buffer length
    int capacity() const {
        return m_BufferLen;
    }

    // get data length in buffer
    uint length() const;
    uint size() const {
        return length();
    }

    // get data in buffer
    char* getBuffer() const {
        return m_Buffer;
    }

    // check if buffer is empty
    bool isEmpty() const {
        return m_Head == m_Tail;
    }

    // How many of the buffered bytes, counted from the head, flush()
    // has already encrypted. Nonzero only between a flush the socket
    // could not take in full and the flush that sends the rest: those
    // bytes go out as they stand, because the encryption key has
    // already moved past them.
    uint encryptedLength() const {
        return m_Encrypted;
    }

    // get debug string
    string toString() const {
        StringStream msg;
        msg << "SocketOutputStream(m_BufferLen:" << m_BufferLen << ",m_Head:" << m_Head << ",m_Tail:" << m_Tail << ")";
        return msg.toString();
    }

    //////////////////////////////////////////////////
    // attributes
    //////////////////////////////////////////////////
private:
    // socket
    Socket* m_Socket;

    // output buffer
    char* m_Buffer;

    // buffer length
    uint m_BufferLen;

    // buffer head/tail
    uint m_Head;
    uint m_Tail;

    // How much of the buffer, starting at the head, is encrypted. A
    // distance from the head rather than a buffer position, so it stays
    // correct across resize(), which moves the buffered bytes to the
    // front of a new allocation but keeps their order.
    uint m_Encrypted;

    // packet sequence number
    BYTE m_Sequence;

    // Encrypt everything between the encrypted region and the tail, so
    // that the whole buffer is ready for the socket. Called by flush()
    // before it sends.
    void encryptPending();

    // writePacket()'s header and body, without the rollback.
    void writeFrame(const Packet* pPacket);

    // Discard every buffered byte past the first `len`, counted from the
    // head. Only bytes not yet encrypted may be discarded.
    void truncate(uint len);

    // add by viva 2008-12-31
public:
    WORD m_EncryptKey;
    BYTE* m_HashTable;
    void setKey(WORD EncryptKey, BYTE* HashTable) {
        m_EncryptKey = EncryptKey;
        m_HashTable = HashTable;
    };
    // The transform applied to the bytes on their way out, and the seam
    // it is exercised through: it returns the key the next run of bytes
    // continues from, so a run split by a short send still produces the
    // bytes one unsplit run would have.
    virtual WORD EncryptData(WORD EncryptKey, char* buf, int len);
    // end
};


//////////////////////////////////////////////////////////////////////
//
// write data to stream (output buffer)
//
// The one copy of the ring-buffer walk: the const char*/uint overload
// and the scalar write<T>() template both come through here. The span
// binds the source to its own length, so the two can no longer
// disagree.
//
// Defined inline, in the header, ON PURPOSE. write<T>() calls this with
// src.size() == sizeof(T); out of line that becomes a call with a runtime
// length, and the memcpy below stops folding to the single store that
// docs/build/toolchain.md section 2 relies on. Every scalar field of every
// packet goes through here, so keep the definition visible.
//
// *Notes*
//
// ( ( m_Head = m_Tail + 1 ) ||
//   ( ( m_Head == 0 ) && ( m_Tail == m_BufferLen - 1 ) )
//
// is what "full" means, so one byte of the buffer is always left
// unused: that is how m_Head == m_Tail can mean "empty" and never
// "full". (The original Korean note did not survive the encoding
// migration.)
//
//////////////////////////////////////////////////////////////////////
inline uint SocketOutputStream::write(std::span<const std::byte> src) {
    __BEGIN_TRY

    const char* buf = reinterpret_cast<const char*>(src.data());
    const uint len = (uint)src.size();

    // Free space left in the buffer. The m_Head > m_Tail case was
    // revised repeatedly -- by sigi. 2002.9.16 / 2002.9.23 / 2002.9.27 --
    // and the dead expression below is what it was revised away from.
    // (The original Korean note did not survive the encoding migration.)
    uint nFree = ((m_Head <= m_Tail) ? m_BufferLen - m_Tail + m_Head - 1 : m_Head - m_Tail - 1);
    // m_Tail - m_Head - 1 );

    // Grow the buffer when the data does not fit in the free space.
    if (len >= nFree)
        resize(len - nFree + 1);

    if (m_Head <= m_Tail) { // normal order

        //
        //    H   T
        // 0123456789
        // ...abcd...
        //

        if (m_Head == 0) {
            nFree = m_BufferLen - m_Tail - 1;
            memcpy(&m_Buffer[m_Tail], buf, len);
        } else {
            nFree = m_BufferLen - m_Tail;
            if (len <= nFree)
                memcpy(&m_Buffer[m_Tail], buf, len);
            else {
                memcpy(&m_Buffer[m_Tail], buf, nFree);
                memcpy(m_Buffer, &buf[nFree], len - nFree);
            }
        }
    } else { // reversed order

        //
        //     T  H
        // 0123456789
        // abcd...efg
        //

        memcpy(&m_Buffer[m_Tail], buf, len);
    }

    // advance m_Tail
    m_Tail = (m_Tail + len) % m_BufferLen;

    return len;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// write a scalar to the output buffer
//
// The ring-buffer walk (and the resize rule that keeps at least one
// byte free, so head == tail can only ever mean "empty") lives in
// the write(std::span<const std::byte>) definition just above --
// inline, so sizeof(T) still reaches its memcpy as a constant.
//
// reinterpret_cast to std::byte* is not an aliasing violation:
// std::byte (like char) may alias any object. std::bit_cast would need
// a second copy through a temporary array to say the same thing, so it
// is not used here.
//
//////////////////////////////////////////////////////////////////////
template <de::WireScalar T> uint SocketOutputStream::write(T buf) {
    return write(std::span<const std::byte>(reinterpret_cast<const std::byte*>(&buf), sizeof(T)));
}

#endif
