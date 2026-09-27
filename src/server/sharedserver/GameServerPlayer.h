//////////////////////////////////////////////////////////////////////
//
// Filename    : GameServerPlayer.h
// Written by  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __GAME_SERVER_PLAYER_H__
#define __GAME_SERVER_PLAYER_H__

// include files
#include "Exception.h"
#include "Player.h"
#include "Types.h"

//////////////////////////////////////////////////////////////////////
//
// class GameServerPlayer
//
//////////////////////////////////////////////////////////////////////

class GameServerPlayer : public Player {
public:
    // constructor
    GameServerPlayer() noexcept(false);
    GameServerPlayer(Socket* pSocket);

    // destructor
    virtual ~GameServerPlayer() noexcept;

    virtual void processInput() noexcept(false);

    virtual void processOutput() noexcept(false);

    // parse packet and execute handler for the packet
    virtual void processCommand() noexcept(false);
    virtual void processCommand(bool Option) {
        Player::processCommand(Option);
    }

    // send packet to player's output buffer
    virtual void sendPacket(Packet* pPacket) noexcept(false);

    // disconnect
    // When closing a player's connection: if it ended abnormally without a
    // logout, the peer socket is already closed, so disconnect(DISCONNECTED) must be used to close
    // it. For a proper logout disconnect(UNDISCONNECTED) must be used
    // instead.
    virtual void disconnect(bool bDisconnected = DISCONNECTED) noexcept(false);

    // get/set socket
    // Socket* getSocket ()  { return m_pSocket; }
    void setSocket(Socket* pSocket) noexcept;

    // get/set player ID
    // string getID () const  { return m_ID; }
    // void setID (const string & id)  { m_ID = id; }

    // get debug string
    virtual string toString() const noexcept(false);

protected:
};

#endif
