//--------------------------------------------------------------------------------
//
// Filename    : LGIncomingConnectionOKHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "LGIncomingConnectionOK.h"

#ifdef __GAME_SERVER__

#include "Assert1.h"
#include "GCReconnectLogin.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "LogDef.h"
#include "PlayerMailbox.h"

#endif

//--------------------------------------------------------------------------------
//
// LGIncomingConnectionOKHander::execute()
//
// When an LGIncomingConnectionOK packet arrives from the login server, the game server has
// to find out which player this permission is for. It then has to throw an LCReconnectLogin
// packet to that player.
//
//--------------------------------------------------------------------------------
void LGIncomingConnectionOKHandler::execute(LGIncomingConnectionOK* pPacket) {
    __BEGIN_DEBUG_EX
#ifdef __GAME_SERVER__

        const string loginServerHost = pPacket->getHost();
    const uint loginServerPort = pPacket->getTCPPort();
    const DWORD key = pPacket->getKey();

    // The player is logging out through the main thread's
    // IncomingPlayerManager, which reads its status and socket, and sends and
    // deletes its reconnect packet when it disconnects it, so the answer is
    // applied on that thread (PlayerMailbox.h, Scope::Player) rather than on
    // this one. The login server names the account, not the character. A
    // player no longer found has nobody to send the address to.
    de::postToAccount(
        pPacket->getPlayerID(),
        [=](PlayerCreature&, Player& player) {
            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(&player);

            if (pGamePlayer == NULL)
                return;

            int fd = -1;
            Socket* pSocket = pGamePlayer->getSocket();
            if (pSocket != NULL)
                fd = (int)pSocket->getSOCKET();

            if (pGamePlayer->getPlayerStatus() == GPS_AFTER_SENDING_GL_INCOMING_CONNECTION) {
                FILELOG_INCOMING_CONNECTION("incomingDisconnect.log", "OK FD : %d, %s", fd,
                                            (pSocket == NULL ? "NULL" : pSocket->getHost().c_str()));

                // Sending the packet here, right away, raced the client's own
                // disconnect. It is stored on the GamePlayer instead, and the
                // IncomingPlayerManager sends it when it disconnects the player.
                GCReconnectLogin* gcReconnectLogin = new GCReconnectLogin;
                gcReconnectLogin->setLoginServerIP(loginServerHost);
                gcReconnectLogin->setLoginServerPort(loginServerPort);
                gcReconnectLogin->setKey(key);

                pGamePlayer->setReconnectPacket(gcReconnectLogin);
            } else {
                FILELOG_INCOMING_CONNECTION("incomingDisconnect.log", "Invalid FD : %d, %s, ps=%d", fd,
                                            (pSocket == NULL ? "NULL" : pSocket->getHost().c_str()),
                                            (int)pGamePlayer->getPlayerStatus());
            }

            // The isPenaltyFlag() check in GamePlayer::processCommand(), which
            // the IncomingPlayerManager runs right after this command, cuts
            // the player off, and the disconnect sends the stored packet.
            pGamePlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
            pGamePlayer->setItemRatioBonusPoint(3);
        },
        nullptr, de::Scope::Player);

#endif

    __END_DEBUG_EX
}
