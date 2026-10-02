//////////////////////////////////////////////////////////////////////
//
// Filename    : Player.h
// Written by  : reiot@ewestsoft.com
// Description : The player class of the game server / login server / test
// client
//
//////////////////////////////////////////////////////////////////////

#ifndef __PLAYER_H__
#define __PLAYER_H__

// include files
#include "Exception.h"
#include "Types.h"

// forward declaration
class Socket;
class SocketInputStream;
class SocketOutputStream;
class Packet;

//////////////////////////////////////////////////////////////////////
//
// class Player
//
// A player is a system object, and one is created per client.
// It holds the TCP socket, the input and output streams over it, and the
// packet sending and handling
// methods. The game server, the login server and the test client each
// inherit from this class and use it.
//
// *CAUTION*
//
// In the game server and the login server in particular, the classes that
// inherit from this class have to synchronise (Mutex Lock/Unlock).
//
//////////////////////////////////////////////////////////////////////

const bool UNDISCONNECTED = true;
const bool DISCONNECTED = false;

class Player {
public:
    // constructor
    Player();
    // Takes ownership of the socket, including if stream construction fails.
    Player(Socket* pSocket);

    // destructor
    virtual ~Player() noexcept(false);

    // read socket's receive buffer and fill input buffer
    virtual void processInput();

    // parse packet and execute handler for the packet
    virtual void processCommand(bool Option = true);

    // flush output buffer to socket's send buffer
    virtual void processOutput();

    // send packet to player's output buffer
    virtual void sendPacket(Packet* pPacket);

    // send stream to player's output buffer
    virtual void sendStream(SocketOutputStream* pOutputStream);

    // disconnect
    // When the player's connection is already closed, or when the logout was
    // not clean, the connection is gone already, so disconnect(DISCONNECTED) has to be used to close the
    // connection. On a clean logout, disconnect(UNDISCONNECTED) has to be used
    // instead.
    virtual void disconnect(bool bDisconnected = DISCONNECTED);

    // get/set socket
    Socket* getSocket() {
        return m_pSocket;
    }
    void setSocket(Socket* pSocket);

    // get/set player ID
    string getID() const {
        return m_ID;
    }
    void setID(const string& id) {
        m_ID = id;
    }

    // get / set ServerGroupID
    ServerGroupID_t getServerGroupID() const {
        return m_ServerGroupID;
    }
    void setServerGroupID(const ServerGroupID_t ServerGroupID) {
        m_ServerGroupID = ServerGroupID;
    }

    // get / set MAC Address
    const BYTE* getMacAddress() const {
        return m_MacAddress;
    }
    void setMacAddress(const BYTE* ma) {
        copy(ma, ma + 6, (BYTE*)m_MacAddress);
    }

    // get debug string
    virtual string toString() const;

    // add by viva
    void setKey(WORD EncryptKey, WORD HashKey);

protected:
    // player id
    string m_ID;

    // TCP client socket
    Socket* m_pSocket;

    // buffered socket input stream
    SocketInputStream* m_pInputStream;

    // buffered socket output stream
    SocketOutputStream* m_pOutputStream;

    // The position of the server this player is connected to
    ServerGroupID_t m_ServerGroupID;

    // MAC Address
    BYTE m_MacAddress[6];

    // add by viva 2008-12-31
    BYTE* pHashTable;
};

#endif
