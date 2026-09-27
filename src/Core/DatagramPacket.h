//////////////////////////////////////////////////////////////////////
//
// Filename    : DatagramPacket.h
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __DATAGRAM_PACKET_H__
#define __DATAGRAM_PACKET_H__

// include files
#include "Datagram.h"
#include "Packet.h"
#include "SocketAPI.h"


//////////////////////////////////////////////////////////////////////
//
// class DatagramPacket;
//
// Base class of the packets used for UDP communication between servers.
// These packets basically have to keep the sender's address and port, and
// have to override the methods that read from and write to a Datagram;
// that is why the class was introduced.
//
//////////////////////////////////////////////////////////////////////

class DatagramPacket : public Packet {
public:
    // destructor
    virtual ~DatagramPacket() {}

    using Packet::read;
    using Packet::write;

    // Read data from the input stream (buffer) and initialise the packet.
    // A datagram packet arriving over a TCP socket counts as a protocol error.
    virtual void read(SocketInputStream& iStream) {
        throw ProtocolException("datagram packet from TCP socket");
    }

    // Read data from the Datagram object and initialise the packet.
    virtual void read(Datagram& iDatagram) = 0;

    // Send the packet's binary image to the output stream (buffer).
    // A datagram packet cannot be written to a TCP socket.
    virtual void write(SocketOutputStream& oStream) const {
        throw Error("cannot write datagram-packet to TCP-socket-stream");
    }

    // Send the packet's binary image to the Datagram object.
    virtual void write(Datagram& oDatagram) const = 0;


    // get packet's DatagramPacketID
    virtual PacketID_t getPacketID() const = 0;

    // get packet's body size
    virtual PacketSize_t getPacketSize() const = 0;

    // get packet's name
    virtual string getPacketName() const = 0;

    // get packet's debug string
    virtual string toString() const = 0;

    // get/set host
    string getHost() const {
        return m_Host;
    }
    void setHost(const string& host) {
        m_Host = host;
    }

    // get/set port
    uint getPort() const {
        return m_Port;
    }
    void setPort(uint port) {
        m_Port = port;
    }


protected:
    // sender's host
    string m_Host;

    // sender's port
    uint m_Port;
};

#endif
