//////////////////////////////////////////////////////////////////////
//
// Filename    : Datagram.cpp
// Written By  : reiot@ewestsoft.com
// Description : Pure wire framing for UDP datagrams. The factory-backed
//               receive path (read(DatagramPacket*&)) lives in
//               DatagramFactoryRead.cpp: it needs PacketFactoryManager,
//               which the kernel must not depend on (rule K1/K2).
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Datagram.h"

#include <stdio.h>

#include <exception>

#include "Assert.h"
#include "DatagramPacket.h"
#include "Packet.h"
#include "Utility.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
Datagram::Datagram() : m_Length(0), m_Capacity(0), m_InputOffset(0), m_OutputOffset(0), m_Data(NULL) {
    __BEGIN_TRY

    memset(&m_SockAddr, 0, sizeof(m_SockAddr));
    m_SockAddr.sin_family = AF_INET;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
Datagram::~Datagram() {
    try {
        if (m_Data != NULL) {
            SAFE_DELETE_ARRAY(m_Data);
            m_Data = NULL;
        }
    } catch (const std::exception&) {
        // ignore during teardown
    }
}


bool Datagram::isDatagram(PacketID_t packetID) {
    switch (packetID) {
    case Packet::PACKET_CG_PORT_CHECK:
    case Packet::PACKET_GG_COMMAND:
    case Packet::PACKET_GG_GUILD_CHAT:
    case Packet::PACKET_GG_SERVER_CHAT:
    case Packet::PACKET_GL_INCOMING_CONNECTION:
    case Packet::PACKET_GL_INCOMING_CONNECTION_ERROR:
    case Packet::PACKET_GL_INCOMING_CONNECTION_OK:
    case Packet::PACKET_GL_KICK_VERIFY:
    case Packet::PACKET_GM_SERVER_INFO:
    case Packet::PACKET_LG_KICK_CHARACTER:
    case Packet::PACKET_LG_INCOMING_CONNECTION:
    case Packet::PACKET_LG_INCOMING_CONNECTION_ERROR:
    case Packet::PACKET_LG_INCOMING_CONNECTION_OK:
        return true;
    default:
        return false;
    }
}

//////////////////////////////////////////////////////////////////////
// Copy data from the internal buffer to an external buffer.
//////////////////////////////////////////////////////////////////////
void Datagram::read(char* buf, uint len) {
    __BEGIN_TRY

    // boundary check
    Assert(m_InputOffset + len <= m_Length);

    memcpy(buf, &m_Data[m_InputOffset], len);

    m_InputOffset += len;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Copy data from the internal buffer to an external string.
//////////////////////////////////////////////////////////////////////
void Datagram::read(string& str, uint len) {
    __BEGIN_TRY

    // boundary check
    Assert(m_InputOffset + len <= m_Length);

    str.reserve(len);
    str.assign(&m_Data[m_InputOffset], len);

    m_InputOffset += len;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Make room for at least len bytes, keeping what the buffer holds.
//
// Growth doubles, so a body that overruns the buffer it was given by a
// long run of small fields reallocates a handful of times rather than
// once per field. Fresh bytes are zeroed: a datagram is padded to
// szPacketHeader + body, and that pad must not be uninitialized memory
// put on the wire.
//
//////////////////////////////////////////////////////////////////////
void Datagram::ensureCapacity(uint len) {
    if (len <= m_Capacity)
        return;

    uint newCapacity = (m_Capacity == 0) ? len : m_Capacity;
    while (newCapacity < len)
        newCapacity *= 2;

    char* pNewData = new char[newCapacity]();

    if (m_Data != NULL) {
        const uint kept = (m_OutputOffset > m_Length) ? m_OutputOffset : m_Length;
        memcpy(pNewData, m_Data, kept);
        SAFE_DELETE_ARRAY(m_Data);
    }

    m_Data = pNewData;
    m_Capacity = newCapacity;
}


//////////////////////////////////////////////////////////////////////
// Copy data from an external buffer into the internal buffer.
//////////////////////////////////////////////////////////////////////
void Datagram::write(const char* buf, uint len) {
    __BEGIN_TRY

    ensureCapacity(m_OutputOffset + len);

    memcpy(&m_Data[m_OutputOffset], buf, len);

    m_OutputOffset += len;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Copy an external string into the internal buffer.
//
// *CAUTION*
//
// Every write() goes through write(const char*,uint), so m_OutputOffset
// does not need adjusting here.
//
//////////////////////////////////////////////////////////////////////
void Datagram::write(const string& str) {
    __BEGIN_TRY

    // write string body
    write(str.c_str(), str.size());

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// write packet
//
// Serializes the packet's binary image into the datagram. Call this on
// the sending side; the datagram's internal buffer must be NULL before
// the call and is allocated by it.
//
// Like SocketOutputStream::writePacket(), the length in the header is
// measured rather than declared: the size field is written as a
// placeholder, the body follows, and the field is then filled in with
// the number of bytes the body produced. getPacketSize() only sizes the
// buffer, so a packet whose declared size has drifted from its write()
// still puts a correct length on the wire.
//
//////////////////////////////////////////////////////////////////////
void Datagram::write(const DatagramPacket* pPacket) {
    __BEGIN_TRY

    Assert(pPacket != NULL);

    const PacketID_t packetID = pPacket->getPacketID();
    const PacketSize_t declaredSize = pPacket->getPacketSize();

    // Size the datagram buffer to the packet's declared size. A body
    // that writes more than that grows the buffer.
    setData(szPacketHeader + declaredSize);

    // Write the packet header: the id, then a placeholder size field.
    const PacketSize_t sizePlaceholder = 0;
    write((char*)&packetID, szPacketID);
    write((char*)&sizePlaceholder, szPacketSize);

    const uint bodyStart = m_OutputOffset;

    // Write the packet body.
    pPacket->write(*this);

    const PacketSize_t bodySize = (PacketSize_t)(m_OutputOffset - bodyStart);

    // Fill the size field in with the measured body length.
    memcpy(&m_Data[szPacketID], &bodySize, szPacketSize);

    // A datagram carries exactly one packet, and both peers measure it
    // as szPacketHeader + body. The header's sequence-byte slot has no
    // meaning on this path and is not written; it travels as a zero pad
    // behind the body, which is what keeps the receiver's length check
    // and the size field in agreement.
    ensureCapacity(szPacketHeader + bodySize);
    m_Length = szPacketHeader + bodySize;

    if (bodySize != declaredSize) {
        // The same report the TCP path makes for the same fault: this
        // packet's getPacketSize() disagrees with its write().
        diagnosticFilelog("packetsizeerror.txt", "Datagram::write: PacketID = %u declared = %u written = %u",
                          (uint)packetID, declaredSize, bodySize);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// set data
//
// Stores data read from the datagram socket into the internal buffer.
//
//////////////////////////////////////////////////////////////////////
void Datagram::setData(char* data, uint len) {
    __BEGIN_TRY

    Assert(data != NULL && m_Data == NULL);

    m_Length = len;
    m_Capacity = len;
    m_Data = new char[m_Capacity];
    memcpy(m_Data, data, m_Length);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Allocate an empty buffer of the given size.
//
// The bytes are zeroed: not every byte of a datagram is necessarily
// written before it is sent, and a datagram must not carry
// uninitialized memory to its peer.
//
//////////////////////////////////////////////////////////////////////
void Datagram::setData(uint len) {
    __BEGIN_TRY

    Assert(m_Data == NULL);

    m_Length = len;
    m_Capacity = len;
    m_Data = new char[m_Capacity]();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// set address
//////////////////////////////////////////////////////////////////////
void Datagram::setAddress(SOCKADDR_IN* pSockAddr) {
    __BEGIN_TRY

    Assert(pSockAddr != NULL);

    memcpy(&m_SockAddr, pSockAddr, szSOCKADDR_IN);


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////
string Datagram::toString() const {
    StringStream msg;
    msg << "Datagram(" << "Length:" << m_Length << ",InputOffset:" << m_InputOffset
        << ",OutputOffset:" << m_OutputOffset << ")";
    return msg.toString();
}
