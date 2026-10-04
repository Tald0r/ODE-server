//////////////////////////////////////////////////////////////////////////////
// Filename    : GamePlyaer.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SharedServerClient.h"

#include <fstream>
#include <memory>

#include "Assert.h"
#include "Guild.h"
#include "GuildManager.h"
#include "KernelContext.h"
#include "PacketDispatcher.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"

//////////////////////////////////////////////////////////////////////////////
// Profile every packet.
//
// To use this,
// MAX_PROFILE_SAMPLES in Profile.h must be raised by 300.
//////////////////////////////////////////////////////////////////////////////
// #define __PROFILE_PACKETS__


//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////

SharedServerClient::SharedServerClient(Socket* pSocket)

    : Player(pSocket) {
    __BEGIN_TRY

    m_Mutex.setName("SharedServerClient");

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////

SharedServerClient::~SharedServerClient() noexcept

{
    __BEGIN_TRY

    __END_CATCH_NO_RETHROW
}


//////////////////////////////////////////////////////////////////////
//
// parse packet and execute handler for the packet
//
//////////////////////////////////////////////////////////////////////
void SharedServerClient::processCommand() {
    __BEGIN_TRY

    // Buffer that temporarily holds the packet header.
    char header[szPacketHeader];
    PacketID_t packetID;
    PacketSize_t packetSize;

    PacketFactoryManager& packetFactories = de::kernelContext().packetFactories();

    try {
        // Process every complete packet in the input buffer.
        while (true) {
            // Peek a packet header's worth of bytes from the input stream.
            // If that many bytes cannot be read from the stream, an
            // Insufficient condition is raised and the loop exits.
            if (!m_pInputStream->peek(&header[0], szPacketHeader)) {
                break;
            }

            // Read the packet id and the packet size.
            // The packet size counts the body only.
            memcpy(&packetID, &header[0], szPacketID);
            memcpy(&packetSize, &header[szPacketID], szPacketSize);

            // An out-of-range packet id is treated as a protocol error.
            if (packetID >= (int)Packet::PACKET_MAX) {
                diagnosticFilelog("SharedServerClient.txt", "Packet ID exceed MAX, RECV [%d/%d]", (int)packetID,
                                  (int)Packet::PACKET_MAX);

                throw InvalidProtocolException("too large packet id");
            }

            try {
                // An oversized packet is treated as a protocol error.
                if (packetSize > packetFactories.getPacketMaxSize(packetID)) {
                    diagnosticFilelog("SharedServerClient.txt", "Too Larget Packet Size, RECV [%d],PacketSize[%d]",
                                      (int)packetID, (int)packetSize);

                    throw InvalidProtocolException("too large packet size");
                }

                // Check that the input buffer holds a whole packet's worth of data.
                if (m_pInputStream->length() < szPacketHeader + packetSize)
                    break;

                // Reaching here means the input buffer holds at least one complete packet.
                // The packet factory manager creates the packet structure from the packet id.
                // A bad packet id is handled by the packet factory manager.
                // The packet lives until its handler returns or throws.
                std::unique_ptr<Packet> pPacket(packetFactories.createPacket(packetID));

                // Now initialize this packet structure.
                // The read() defined by the packet subclass is called through the virtual
                // mechanism and fills it in.
                m_pInputStream->readPacket(pPacket.get());

// Now run the packet handler on this packet structure.
// A bad packet id is handled by the packet handler manager.
#ifdef __PROFILE_PACKETS__

                beginProfileEx(pPacket->getPacketName().c_str());
                PacketDispatcher::dispatch(pPacket.get(), this);
                endProfileEx(pPacket->getPacketName().c_str());

#else
                PacketDispatcher::dispatch(pPacket.get(), this);
#endif
            } catch (IgnorePacketException& igpe) {
                // An oversized packet is treated as a protocol error.
                if (packetSize > packetFactories.getPacketMaxSize(packetID))
                    throw InvalidProtocolException("too large packet size");

                // Check that the input buffer holds a whole packet's worth of data.
                if (m_pInputStream->length() < szPacketHeader + packetSize)
                    break;

                // Once all the data has arrived, skip that many bytes and
                // go on to the next packet.
                m_pInputStream->skip(szPacketHeader + packetSize);

                // A skipped packet does not affect expiry:
                // only valid packets are kept from being cut off.
                // It does not go into the history either.
            }
        }
    } catch (InsufficientDataException& ide) {
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// flush output buffer to socket's send buffer
//
// No other thread may call sendPacket on the output buffer while it is flushing.
// (The only such case is a say arriving over the inter-server link.)
//
//////////////////////////////////////////////////////////////////////
void SharedServerClient::processOutput() {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    try {
        Player::processOutput();
    } catch (InvalidProtocolException& It) {
        throw DisconnectException("Pipe broken; closing the connection");
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// send packet to player's output buffer
//
//////////////////////////////////////////////////////////////////////
void SharedServerClient::sendPacket(Packet* pPacket) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    try {
        Player::sendPacket(pPacket);
    } catch (InvalidProtocolException& It) {
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// get debug string
//
//////////////////////////////////////////////////////////////////////
string SharedServerClient::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    //////////////////////////////////////////////////
    // enter critical section
    //////////////////////////////////////////////////
    __ENTER_CRITICAL_SECTION(m_Mutex)

    msg << "SharedServerClient(SocketID:" << m_pSocket->getSOCKET() << ",Host:" << m_pSocket->getHost() << ")";

    //////////////////////////////////////////////////
    // leave critical section
    //////////////////////////////////////////////////
    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return msg.toString();

    __END_CATCH
}
