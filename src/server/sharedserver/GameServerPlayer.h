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

    void processInput() noexcept(false) override;

    void processOutput() noexcept(false) override;

    // parse packet and execute handler for the packet
    void processCommand(bool Option = true) noexcept(false) override;

    // send packet to player's output buffer
    void sendPacket(Packet* pPacket) noexcept(false) override;

    // disconnect
    // When closing a player's connection: if it ended abnormally without a
    // logout, the peer socket is already closed, so disconnect(DISCONNECTED) must be used to close
    // it. For a proper logout disconnect(UNDISCONNECTED) must be used
    // instead.
    void disconnect(bool bDisconnected = DISCONNECTED) noexcept(false) override;

    // get/set player ID
    // string getID () const  { return m_ID; }
    // void setID (const string & id)  { m_ID = id; }

    // get debug string
    string toString() const noexcept(false) override;

protected:
};

#endif
