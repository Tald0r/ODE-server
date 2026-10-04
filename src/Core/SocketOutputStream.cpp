//////////////////////////////////////////////////////////////////////
//
// SocketOutputStream.cpp
//
// by Reiot
//
//////////////////////////////////////////////////////////////////////

#include "SocketOutputStream.h"

#include "Assert.h"
#include "Packet.h"
#include "VSDateTime.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
SocketOutputStream::SocketOutputStream(Socket* sock, uint BufferLen)
    : m_Socket(sock), m_Buffer(NULL), m_BufferLen(BufferLen), m_Head(0), m_Tail(0), m_Encrypted(0), m_Sequence(0) {
    __BEGIN_TRY

    Assert(m_BufferLen > 0);

    m_Buffer = new char[m_BufferLen];

    // add by viva
    m_EncryptKey = 0;
    m_HashTable = NULL;
    // end

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
SocketOutputStream::~SocketOutputStream() noexcept {
    if (m_Buffer != NULL) {
        // After a ConnectException has been caught because the connection broke,
        // flushing here would raise SIGPIPE. So, leave it alone~
        delete[] m_Buffer;
        m_Buffer = NULL;
    }
}


//////////////////////////////////////////////////////////////////////
// write data to stream (legacy pointer/length entry point)
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::write(const char* buf, uint len) {
    return write(std::span<const std::byte>(reinterpret_cast<const std::byte*>(buf), len));
}


//////////////////////////////////////////////////////////////////////
// reserve a field to be filled in later
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::reserveField(uint len) {
    Assert(len > 0);

    // Taken before the placeholder bytes are written, so the handle is
    // the distance from the head to the START of the field.
    const uint slot = length();

    for (uint i = 0; i < len; i++)
        write((BYTE)0);

    return slot;
}


//////////////////////////////////////////////////////////////////////
// fill in a reserved field
//////////////////////////////////////////////////////////////////////
void SocketOutputStream::patchField(uint slot, std::span<const std::byte> src) {
    Assert(slot + (uint)src.size() <= length());

    // A field must not be patched after the bytes it sits in have been
    // encrypted, which would put plaintext into an encrypted region.
    // Reservations are made and filled while a packet is written, and
    // nothing flushes in between, so the field always sits past the
    // encrypted region.
    Assert(slot >= m_Encrypted);

    // The field can sit anywhere in the ring, wrap point included, so it
    // is walked byte by byte rather than split into two copies -- fields
    // patched this way are a few bytes wide.
    uint position = (m_Head + slot) % m_BufferLen;

    for (size_t i = 0; i < src.size(); i++) {
        m_Buffer[position] = (char)src[i];
        position = (position + 1 == m_BufferLen) ? 0 : position + 1;
    }
}


//////////////////////////////////////////////////////////////////////
// write packet to stream (output buffer)
//
// The size field of the header is reserved before the body is written
// and filled in afterwards with the number of bytes the body produced,
// so the length on the wire is measured rather than declared and a
// getPacketSize() that has drifted from write() cannot desynchronise the
// stream. getPacketSize() is still what the buffers around this call are
// sized from, so a disagreement is reported.
//
// A packet is written whole or not at all. When the body refuses a field
// (a string past its bound throws InvalidProtocolException) or anything
// else throws on the way, the header and the fields already written are
// taken back out of the stream and the sequence counter returns to its
// value before the call, so the stream holds exactly what it held before
// and the next packet goes out as if the refused one had never been sent.
//////////////////////////////////////////////////////////////////////
void SocketOutputStream::writePacket(const Packet* pPacket) {
    __BEGIN_TRY

    // Where this packet begins, as a distance from the head, which no
    // write moves and resize() keeps.
    const uint packetStart = length();
    const BYTE packetSequence = m_Sequence;

    try {
        writeFrame(pPacket);
    } catch (...) {
        truncate(packetStart);
        m_Sequence = packetSequence;
        throw;
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// write one packet's header and body -- writePacket()'s work, which it
// takes back out of the stream if this throws
//////////////////////////////////////////////////////////////////////
void SocketOutputStream::writeFrame(const Packet* pPacket) {
    PacketID_t packetID = pPacket->getPacketID();
    Assert(packetID != 0);

    write((char*)&packetID, szPacketID);

    const uint sizeSlot = reserveField(szPacketSize);

    // The sequence.
    write((char*)&m_Sequence, szSequenceSize);
    m_Sequence++;

    const uint bodyStart = length();

    pPacket->write(*this);

    const PacketSize_t bodySize = (PacketSize_t)(length() - bodyStart);

    patchField(sizeSlot, bodySize);

    const PacketSize_t declaredSize = pPacket->getPacketSize();

    if (bodySize != declaredSize) {
        // Reported to the file resize() already uses for the same fault:
        // a server's stdout is normally discarded, and both messages mean
        // "this packet's getPacketSize() disagrees with its write()".
        diagnosticFilelog("packetsizeerror.txt", "writePacket: PacketID = %u declared = %u written = %u",
                          (uint)packetID, declaredSize, bodySize);
    }

    cout << "Send:" << packetID << "[" << bodySize << "," << (m_Sequence - 1) << "]" << " " << pPacket->toString()
         << endl;
}


//////////////////////////////////////////////////////////////////////
// discard every byte past the first `len`
//////////////////////////////////////////////////////////////////////
void SocketOutputStream::truncate(uint len) {
    Assert(len <= length());

    // Encrypted bytes are already committed to the key chain.
    Assert(len >= m_Encrypted);

    m_Tail = (m_Head + len) % m_BufferLen;
}


//////////////////////////////////////////////////////////////////////
// encrypt everything between the encrypted region and the tail
//
// The encrypted region always begins at the head, so this extends it to
// cover the whole buffer. The key advances one step per byte, so a run
// encrypted in several pieces -- which is what writes made after a
// short send produce -- comes out as the bytes one unsplit run would
// have produced, and the receiver's key chain undoes it in one pass.
//////////////////////////////////////////////////////////////////////
void SocketOutputStream::encryptPending() {
    const uint nPlain = length() - m_Encrypted;

    if (nPlain == 0)
        return;

    // The plaintext region can straddle the buffer's wrap point.
    const uint position = (m_Head + m_Encrypted) % m_BufferLen;
    const uint nBeforeWrap = m_BufferLen - position;
    const uint nFirst = (nPlain < nBeforeWrap) ? nPlain : nBeforeWrap;

    m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[position], nFirst);

    if (nFirst < nPlain)
        m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[0], nPlain - nFirst);

    m_Encrypted += nPlain;
}


//////////////////////////////////////////////////////////////////////
// flush stream (output buffer) to socket
//
// Each byte is encrypted exactly once, before its first send. That is
// what the encrypted-byte count is for: a non-blocking socket whose
// peer has stopped reading takes only part of the buffer and reports
// zero for the rest, and the bytes left behind are already encrypted.
// Encrypting them again would put bytes on the wire that the receiver's
// key chain cannot undo -- neither those bytes nor anything sent after
// them, since the key advances per byte.
//
// A send never spans the wrap point, so a buffer whose contents wrap
// goes out in two calls, and a send that stops short anywhere in either
// of them leaves the head, the tail and the encrypted-byte count
// describing exactly what is left to send.
//
// Nothing throws NonBlockingIOException on this path: SocketAPI::send_ex
// returns zero for EWOULDBLOCK and converts every other failure to
// InvalidProtocolException, which propagates with the buffer state
// already consistent.
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::flush() {
    __BEGIN_TRY

    Assert(m_Socket != NULL);

    encryptPending();

    uint nFlushed = 0;

    while (m_Head != m_Tail) {
        //
        //    H   T          or          T  H
        // 0123456789               0123456789
        // ...abcd...               abcd...efg
        //
        const uint nContiguous = (m_Head < m_Tail) ? m_Tail - m_Head : m_BufferLen - m_Head;

        const uint nSent = m_Socket->send(&m_Buffer[m_Head], nContiguous, MSG_NOSIGNAL);

        // The socket cannot take any more for now. What is left stays
        // buffered, and stays encrypted.
        if (nSent == 0)
            return nFlushed;

        Assert(nSent <= nContiguous);

        nFlushed += nSent;
        m_Head = (m_Head + nSent) % m_BufferLen;
        m_Encrypted -= nSent;
    }

    // Empty: restart at the front of the buffer, so the next writes
    // stay contiguous for as long as possible.
    m_Head = m_Tail = 0;
    m_Encrypted = 0;

    return nFlushed;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// resize buffer
//////////////////////////////////////////////////////////////////////
void SocketOutputStream::resize(int size) {
    __BEGIN_TRY

    Assert(size != 0);

    int orgSize = size;

    // To avoid frequent resizes.. grow by half the current size by sigi. 2002.9.26
    size = max(size, (int)(m_BufferLen >> 1));
    uint newBufferLen = m_BufferLen + size;
    uint len = length();

    if (size < 0) {
        // The buffer size is being reduced but the data in the buffer
        // would not fit
        if (newBufferLen < 0 || newBufferLen < len)
            throw IOException("new buffer is too small!");
    }

    // Allocate a new buffer.
    char* newBuffer = new char[newBufferLen];

    // Copy the data in the existing buffer.
    if (m_Head < m_Tail) {
        //
        //    H   T
        // 0123456789
        // ...abcd...
        //

        memcpy(newBuffer, &m_Buffer[m_Head], m_Tail - m_Head);
    } else if (m_Head > m_Tail) {
        //
        //     T  H
        // 0123456789
        // abcd...efg
        //

        memcpy(newBuffer, &m_Buffer[m_Head], m_BufferLen - m_Head);
        memcpy(&newBuffer[m_BufferLen - m_Head], m_Buffer, m_Tail);
    }

    // Delete the old buffer.
    delete[] m_Buffer;

    // Point the stream at the new buffer.
    //
    // The bytes were copied in order and the head is back at the front,
    // so the encrypted region -- a distance from the head -- still
    // covers the same bytes and is left alone.
    m_Buffer = newBuffer;
    m_BufferLen = newBufferLen;
    m_Head = 0;
    m_Tail = len;

    VSDateTime current = VSDateTime::currentDateTime();

    if (m_Socket == NULL) {
        // m_Socket being NULL means this stream is a broadcast stream.
        // A resize being called means the packet's getPacketSize() function is wrong.
        diagnosticFilelog("packetsizeerror.txt", "PacketID = %u", *(PacketID_t*)m_Buffer);
    } else {
        ofstream ofile("buffer_resized.log", ios::app);
        ofile << "[" << current.toString().c_str() << "] " << m_Socket->getHost().c_str()
              << " - SocketOutputStream resized: " << orgSize << " / " << size << "/" << m_BufferLen << " bytes!"
              << endl;
        ofile.close();
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get data's size in buffer
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::length() const {
    if (m_Head < m_Tail)
        return m_Tail - m_Head;

    else if (m_Head > m_Tail)
        return m_BufferLen - m_Head + m_Tail;

    return 0;
}

// add by viva 2008-12-31
WORD SocketOutputStream::EncryptData(WORD EncryptKey, char* buf, int len) {
    return EncryptKey;

    for (int i = 0; i < len; i++)
        *(buf + i) ^= 0xCC;

    if (m_HashTable == NULL)
        return EncryptKey;

    for (int i = 0; i < len; i++) {
        *(buf + i) ^= m_HashTable[EncryptKey];
        if (++EncryptKey == 512)
            EncryptKey = 0;
    }

    return EncryptKey;
}
// end
