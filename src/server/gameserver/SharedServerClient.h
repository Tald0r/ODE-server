//////////////////////////////////////////////////////////////////////////////
// Filename    : SharedServerClient.h
// Written by  : reiot@ewestsoft.com
// Description : Player class for the game server
//////////////////////////////////////////////////////////////////////////////

#ifndef __SHARED_SERVER_CLIENT_H
#define __SHARED_SERVER_CLIENT_H

#include "Exception.h"
#include "Mutex.h"
#include "Packet.h"
#include "Player.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////////////
// class SharedServerClient
//
// Shared server client player for the game server
//
//////////////////////////////////////////////////////////////////////////////

class SharedServerClient : public Player {
public:
    // Number of previous packets to keep
    const static BYTE nPacketHistorySize = 10;

public:
    SharedServerClient(Socket* pSocket);
    ~SharedServerClient() noexcept;

public:
    // read socket's receive buffer and fill input buffer
    // virtual void processInput() ;

    // parse packet and execute handler for the packet
    virtual void processCommand();
    virtual void processCommand(bool Option) {
        Player::processCommand(Option);
    }

    // flush output buffer to socket's send buffer
    virtual void processOutput();

    // send packet to player's output buffer
    virtual void sendPacket(Packet* packet);

    // get debug string
    virtual string toString() const;


private:
    // mutex
    mutable Mutex m_Mutex;
};

#endif
