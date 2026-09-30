//////////////////////////////////////////////////////////////////////
//
// SocketInputStream.cpp
//
// by Reiot
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////
#include "SocketInputStream.h"

#include <errno.h>

#include "Assert.h"
#include "Packet.h"

#if __LINUX__
#include <sys/ioctl.h>
#endif


//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
SocketInputStream::SocketInputStream(Socket* sock, uint BufferLen)
    : m_pSocket(sock), m_Buffer(NULL), m_BufferLen(BufferLen), m_Head(0), m_Tail(0) {
    __BEGIN_TRY

    Assert(m_pSocket != NULL);
    Assert(m_BufferLen > 0);

    m_Buffer = new char[m_BufferLen];
    // add by viva 2008-12-31
    m_EncryptKey = 0;
    m_HashTable = NULL;
    // end

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
SocketInputStream::~SocketInputStream() noexcept {
    SAFE_DELETE_ARRAY(m_Buffer);
}


//////////////////////////////////////////////////////////////////////
//
// read data from input buffer (legacy pointer/length entry point)
//
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::read(char* buf, uint len) {
    // Checked before the span is formed: a null buffer must still be the
    // assertion it has always been, not a span over a null pointer.
    Assert(buf != NULL);

    return read(std::span<std::byte>(reinterpret_cast<std::byte*>(buf), len));
}


//////////////////////////////////////////////////////////////////////
// read data from input buffer
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::read(string& str, uint len) {
    __BEGIN_TRY

    if (len == 0)
        failRead("len==0");

    // Less data buffered, or left in the open frame's body, than was
    // asked for.
    if (len > length())
        failUnderflow(len - length());

    // Reserve len bytes in the string up front.
    str.reserve(len);

    if (m_Head < m_Tail) { // normal order

        //
        //    H   T
        // 0123456789
        // ...abcd...
        //

        str.assign(&m_Buffer[m_Head], len);
    } else { // reversed order ( m_Head > m_Tail )

        //
        //     T  H
        // 0123456789
        // abcd...efg
        //

        uint rightLen = m_BufferLen - m_Head;
        if (len <= rightLen) {
            str.assign(&m_Buffer[m_Head], len);
        } else {
            str.assign(&m_Buffer[m_Head], rightLen);
            str.append(m_Buffer, len - rightLen);
        }
    }

    m_Head = (m_Head + len) % m_BufferLen;

    return len;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// refuse a stream operation
//
// Inside an open frame the failure is recorded before it is thrown, so
// readPacket() refuses the frame even when the packet's read() catches
// the exception and carries on.
//////////////////////////////////////////////////////////////////////
void SocketInputStream::failRead(const char* message) {
    if (m_bFrameBounded)
        m_bFrameReadFailed = true;
    throw InvalidProtocolException(message);
}

void SocketInputStream::failUnderflow(uint missing) {
    if (m_bFrameBounded)
        m_bFrameReadFailed = true;
    throw InsufficientDataException(missing);
}


//////////////////////////////////////////////////////////////////////
// close the open frame
//
// The head moves straight to the end of the body, however much of it
// the packet read: skip() would refuse the zero-byte remainder of a
// body read exactly.
//////////////////////////////////////////////////////////////////////
void SocketInputStream::finishFrame() noexcept {
    m_Head = m_FrameEnd;
    m_Tail = m_FrameTail;
    m_bFrameBounded = false;
    m_bFrameReadFailed = false;
}


//////////////////////////////////////////////////////////////////////
// read packet from input buffer
//
// The receive loops pick the packet from the header's id and call this
// once the whole frame is buffered; it checks that again, so it is safe
// for any caller. The header's size bounds the packet's read(): m_Tail
// is pulled in to the end of the body while read() runs, so length(),
// and with it every read, peek and skip, stops there.
//////////////////////////////////////////////////////////////////////
void SocketInputStream::readPacket(Packet* pPacket) {
    __BEGIN_TRY

    // A packet read inside another packet's read would take its header
    // from the outer body. Refused without touching the stream; the
    // failure is recorded, so the outer frame is refused as well.
    if (m_bFrameBounded)
        failRead("nested packet read");

    Assert(pPacket != NULL);

    // Nothing is consumed until the header and the whole declared body
    // are buffered: a fragmented frame waits whole for the next fill().
    char header[szPacketHeader];
    if (!peek(header, szPacketHeader))
        throw InsufficientDataException(szPacketHeader - length());

    PacketSize_t packetSize = 0;
    memcpy(&packetSize, &header[szPacketID], szPacketSize);
    const uint bufferedBody = length() - szPacketHeader;
    if (packetSize > bufferedBody)
        throw InsufficientDataException(packetSize - bufferedBody);

    // The id picked the packet and the size bounds it; the sequence byte
    // is the receive loop's to check. Open the frame over the body.
    m_Head = (m_Head + szPacketHeader) % m_BufferLen;
    m_FrameEnd = (m_Head + packetSize) % m_BufferLen;
    m_FrameTail = m_Tail;
    m_Tail = m_FrameEnd;
    m_bFrameBounded = true;
    m_bFrameReadFailed = false;

    try {
        pPacket->read(*this);
    } catch (InsufficientDataException&) {
        // The whole body was buffered, so a field that runs past it is a
        // malformed body, not a fragment the receive loop may wait for.
        finishFrame();
        throw InvalidProtocolException("packet read ran past its declared body");
    } catch (IgnorePacketException&) {
        // A receive loop answers this by skipping the frame, which is
        // consumed already: it would skip the next one instead.
        finishFrame();
        throw InvalidProtocolException("packet read asked for its frame to be ignored");
    } catch (...) {
        finishFrame();
        throw;
    }

    const uint unread = length();
    const bool failed = m_bFrameReadFailed;
    finishFrame();

    if (failed)
        throw InvalidProtocolException("packet read caught a stream failure");
    if (unread != 0)
        throw InvalidProtocolException("packet read left its declared body unread");

    cout << "Receive:" << pPacket->toString() << endl;
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// peek data from buffer
//////////////////////////////////////////////////////////////////////
bool SocketInputStream::peek(std::span<std::byte> dst) {
    char* buf = reinterpret_cast<char*>(dst.data());
    const uint len = (uint)dst.size();

    Assert(buf != NULL);

    if (len == 0)
        failRead("len==0");

    // Less data buffered than asked for. Unlike read() that is not an
    // exception here: "not yet" is peek()'s normal answer. Inside an open
    // frame the whole body is buffered, so a peek past it is refused
    // instead: the bytes behind it are the next frame's.
    if (len > length()) {
        if (m_bFrameBounded)
            failRead("packet read peeked past its declared body");
        return false;
    }

    // Copy into buf, but leave m_Head where it is.
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
            memcpy(&buf[0], &m_Buffer[m_Head], len);
        } else {
            memcpy(&buf[0], &m_Buffer[m_Head], rightLen);
            memcpy(&buf[rightLen], &m_Buffer[0], len - rightLen);
        }
    }

    return true;
}


//////////////////////////////////////////////////////////////////////
// peek data from buffer (legacy pointer/length entry point)
//////////////////////////////////////////////////////////////////////
bool SocketInputStream::peek(char* buf, uint len) {
    Assert(buf != NULL);

    return peek(std::span<std::byte>(reinterpret_cast<std::byte*>(buf), len));
}


//////////////////////////////////////////////////////////////////////
//
// skip data from buffer
//
// read(N) == peek(N) + skip(N)
//
//////////////////////////////////////////////////////////////////////
void SocketInputStream::skip(uint len) {
    __BEGIN_TRY

    if (len == 0)
        failRead("len==0");

    if (len > length())
        failUnderflow(len - length());

    // Advance m_Head.
    m_Head = (m_Head + len) % m_BufferLen;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// fill buffer from socket
//
// When there is no data, 0 is returned.
// With NonBlocking, though, a NonBlockingIOException is thrown.
//
// *NOTES*
//
// The usual way is to declare char buf[N]; in the calling function, receive()
// into it and then copy it into the buffer.
// That way the copy happens twice, though, so it is done
// differently here.
//
// So the stream receives straight into its own buffer, and because that
// buffer is circular, in the normal order there is a good chance
// receive() is called more than once.
//
// The first receive() then reads all the data in the receive buffer,
// and if nothing arrives at the same time the second receive() raises
// a NonBlockingIOException. (this is rare. it happens when data
// arrives right behind the first one and is left over
// by chance..)
// )
//
// So. should fill() throw the NonBlockingIOException out, in that case?
// Or should it return the size of the data read so far? Hard to say. ^^;
// Let us leave it like this~~~ go with the flow~
//
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::fill() {
    __BEGIN_TRY

    uint nFilled = 0; // size filled into the buffer
    uint nReceived;   // size read in one Socket::receive()
    uint nFree;       // size of the free room left

    if (m_Head <= m_Tail) { // normal order
        // m_Head == m_Tail means the buffer is empty.

        // First fill the room up to the end of the buffer.
        if (m_Head == 0) {
            //
            // H   T
            // 0123456789
            // abcd......
            //

            // Note that when m_Head == 0, m_Tail could wrap round to 0 again and
            // the buffer would look empty. So m_Head is checked and, when it
            // is 0, one slot before the end is left free for m_Tail. ^^

            nFree = m_BufferLen - m_Tail - 1;
            nReceived = m_pSocket->receive(&m_Buffer[m_Tail], nFree);

            // by sigi. NonblockException handling. 2002.5.17
            if (nReceived == 0)
                return 0;
            // add by viva
            if (nReceived > 0)
                m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[m_Tail], nReceived);
            // end
            m_Tail += nReceived;
            nFilled += nReceived;

            if (nReceived == nFree) {
                // There may be data left in the receive buffer of the socket.
                // The input buffer has no room left, though, so if there
                // is data left the buffer has to be grown.
                uint available = m_pSocket->available();
                if (available > 0) {
                    resize(available + 1);
                    // After a resize the data is aligned again, so m_Tail changes too.
                    nReceived = m_pSocket->receive(&m_Buffer[m_Tail], available);

                    // by sigi. NonblockException handling. 2002.5.17
                    if (nReceived == 0)
                        return 0;
                    // add by viva
                    if (nReceived > 0)
                        m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[m_Tail], nReceived);
                    // end
                    m_Tail += nReceived;
                    nFilled += nReceived;
                }
            }
        } else { // m_Head != 0

            //
            //    H   T
            // 0123456789
            // ...abcd...
            //

            // In that case m_Tail cannot pass the end of the
            // buffer.
            nFree = m_BufferLen - m_Tail;
            nReceived = m_pSocket->receive(&m_Buffer[m_Tail], nFree);

            // by sigi. NonblockException handling. 2002.5.17
            if (nReceived == 0)
                return 0;
            // add by viva
            if (nReceived > 0)
                m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[m_Tail], nReceived);
            // end
            m_Tail = (m_Tail + nReceived) % m_BufferLen;
            nFilled += nReceived;

            if (nReceived == nFree) {
                Assert(m_Tail == 0);

                // There may be more data left in the socket's receive buffer.
                // So the data has to be received into the front of the input buffer.
                // m_Head == m_Tail means empty, though, so
                // one byte is left spare.
                nFree = m_Head - 1;
                nReceived = m_pSocket->receive(&m_Buffer[0], nFree);

                // by sigi. NonblockException handling. 2002.5.17
                if (nReceived == 0)
                    return 0;
                // add by viva
                if (nReceived > 0)
                    m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[m_Tail], nReceived);
                // end
                m_Tail += nReceived;
                nFilled += nReceived;

                if (nReceived == nFree) { // buffer is full

                    // When the buffer is completely full there may be more data left in the
                    // socket's receive buffer. So, to read it,
                    // the buffer is grown.
                    uint available = m_pSocket->available();
                    if (available > 0) {
                        resize(available + 1);
                        // After a resize the data is aligned again, so m_Tail changes too.
                        nReceived = m_pSocket->receive(&m_Buffer[m_Tail], available);

                        // by sigi. NonblockException handling. 2002.5.17
                        if (nReceived == 0)
                            return 0;
                        // add by viva
                        if (nReceived > 0)
                            m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[m_Tail], nReceived);
                        // end
                        m_Tail += nReceived;
                        nFilled += nReceived;
                    }
                }
            }
        }
    } else { // reversed order ( m_Head > m_Tail )

        //
        //     T  H
        // 0123456789
        // abcd...efg
        //

        nFree = m_Head - m_Tail - 1;
        nReceived = m_pSocket->receive(&m_Buffer[m_Tail], nFree);

        // by sigi. NonblockException handling. 2002.5.17
        if (nReceived == 0)
            return 0;
        // add by viva
        if (nReceived > 0)
            m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[m_Tail], nReceived);
        // end
        m_Tail += nReceived;
        nFilled += nReceived;

        if (nReceived == nFree) { // buffer is full

            // In that case there may be more data left in the socket's receive
            // buffer. So, to read it, the buffer is
            // grown.
            uint available = m_pSocket->available();
            if (available > 0) {
                resize(available + 1);
                // After a resize the data is aligned again, so m_Tail changes too.
                nReceived = m_pSocket->receive(&m_Buffer[m_Tail], available);

                // by sigi. NonblockException handling. 2002.5.17
                if (nReceived == 0)
                    return 0;
                // add by viva
                if (nReceived > 0)
                    m_EncryptKey = EncryptData(m_EncryptKey, &m_Buffer[m_Tail], nReceived);
                // end
                m_Tail += nReceived;
                nFilled += nReceived;
            }
        }
    }
    return nFilled;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// fill buffer from socket
//
// Byte mode - it may be a little slow. 0 -;
//
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::fill_RAW() {
    __BEGIN_TRY

#if __LINUX__
    uint nfree = m_BufferLen - m_Tail - 1;

    int nread = recv(m_pSocket->getSOCKET(), &m_Buffer[m_Tail], nfree, 0);

    if (nread < 0) {
        if (errno == EWOULDBLOCK) {
            // NonBlockingIOException
            nread = 0;
        } else if (errno == ECONNRESET) {
            // ConnectException
            throw ConnectException();
        } else {
            // Error
            throw UnknownError(strerror(errno), errno);
        }
    } else if (nread == 0) {
        // EOFException
        throw EOFException();
    }

    m_Tail += nread;

    if (nread == (int)nfree) {
        // There may be more data.
        uint more = 0;
        int result = ioctl(m_pSocket->getSOCKET(), FIONREAD, &more);
        if (result < 0)
            throw UnknownError(strerror(errno), errno);

        if (more > 0) {
            // Grow the buffer.
            resize(more + 1);

            // Fill the buffer.
            nread = recv(m_pSocket->getSOCKET(), &m_Buffer[m_Tail], more, 0);

            // It has to read exactly more bytes. If it does not,
            // something is wrong.
            Assert((int)more == nread);

            nread += more;
        }
    }

    return nread;

#else
    throw UnsupportedError();
#endif

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// resize buffer
//////////////////////////////////////////////////////////////////////
void SocketInputStream::resize(int size) {
    __BEGIN_TRY

    Assert(size != 0);

    // resize size related. by sigi. 2002.10.7
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

    // Reset the buffer and its size.
    m_Buffer = newBuffer;
    m_BufferLen = newBufferLen;
    m_Head = 0;
    m_Tail = len; // m_Tail is set to the length of the data held.

    ofstream ofile("buffer_resized.log", ios::app);
    ofile << "SocketInputStream resized " << size << " bytes!" << endl;
    ofile.close();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// get data's size in buffer
//
// NOTES
//
//       H   T           T  H
//    0123456789     0123456789
//    ...abcd...     abcd...efg
//
//    7 - 3 = 4      10 - ( 7 - 4 ) = 7
//
// CAUTION
//
//    Note that m_Tail points at an empty slot.
//    So if the buffer size is m_BufferLen, the data the queue can
//    hold is ( m_BufferLen - 1 ).
//
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::length() const {
    __BEGIN_TRY

    if (m_Head < m_Tail)
        return m_Tail - m_Head;

    else if (m_Head > m_Tail)
        return m_BufferLen - m_Head + m_Tail;

    return 0;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////
string SocketInputStream::toString() const {
    StringStream msg;

    msg << "SocketInputStream(" << "BufferLen:" << m_BufferLen << ",Head:" << m_Head << ",Tail:" << m_Tail << ")";

    return msg.toString();
}
// add by viva 2008-12-31
WORD SocketInputStream::EncryptData(WORD EncryptKey, char* buf, int len) {
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
