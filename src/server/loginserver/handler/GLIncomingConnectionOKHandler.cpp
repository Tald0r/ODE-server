//--------------------------------------------------------------------------------
//
// Filename    : GLIncomingConnectionOKHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "GLIncomingConnectionOK.h"

#ifdef __LOGIN_SERVER__

#include "Assert1.h"
#include "LCReconnect.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginPlayerManager.h"

#endif

//--------------------------------------------------------------------------------
//
// GLIncomingConnectionOKHander::execute()
//
// When a GLIncomingConnectionOK packet arrives from the game server, the login server
// has to find which player the permission is for and then throw an LCReconnect
// packet at that player.
//
//--------------------------------------------------------------------------------
void GLIncomingConnectionOKHandler::execute(GLIncomingConnectionOK* pPacket)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX
#ifdef __LOGIN_SERVER__

        try {
        LoginPlayerManager& loginPlayers = de::loginContext().loginPlayers();

        // Reach the player object through the player id.
        __ENTER_CRITICAL_SECTION(loginPlayers)

        LoginPlayer* pLoginPlayer = loginPlayers.getPlayer_NOLOCKED(pPacket->getPlayerID());

        if (pLoginPlayer->getPlayerStatus() == LPS_AFTER_SENDING_LG_INCOMING_CONNECTION) {
            // Tell the client to reconnect the game server.
            // by tiancaiamao: when gameserver is behind docker, it may have a docker internal IP 172.20.0.1 and a
            // outside IP in database GameServerInfo table. The outside IP should be used. pPacket->getHost() get the
            // internal one. pLoginPlayer->getGameServerIP() get the outside one.

            LCReconnect lcReconnect;
            lcReconnect.setGameServerIP(pLoginPlayer->getGameServerIP());
            lcReconnect.setGameServerPort(pPacket->getTCPPort());
            lcReconnect.setKey(pPacket->getKey());

            // Send the LCReconnect packet.
            pLoginPlayer->sendPacket(&lcReconnect);
        } else {
        }

        loginPlayers.retirePlayer_NOLOCKED(pLoginPlayer->getSocket()->getSOCKET(), UNDISCONNECTED);

        __LEAVE_CRITICAL_SECTION(loginPlayers)
    } catch (NoSuchElementException& nsee) {
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
