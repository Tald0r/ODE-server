//--------------------------------------------------------------------------------
//
// Filename    : LGIncomingConnectionErrorHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "LGIncomingConnectionError.h"

#ifdef __GAME_SERVER__
#include "Assert.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "LogDef.h"
#include "PlayerMailbox.h"
#endif

//--------------------------------------------------------------------------------
//
// LGIncomingConnectionErrorHander::execute()
//
//--------------------------------------------------------------------------------
void LGIncomingConnectionErrorHandler::execute(LGIncomingConnectionError* pPacket) {
    __BEGIN_DEBUG_EX
#ifdef __GAME_SERVER__

        const string playerID = pPacket->getPlayerID();

    // The login server refused the player's return to it, so the player is
    // cut off. It is logging out through the main thread's
    // IncomingPlayerManager, which owns its status, socket and flags, so the
    // kick is applied on that thread (PlayerMailbox.h, Scope::Player); the
    // disconnect follows in the same pass. The login server names the
    // account, not the character.
    const bool found = de::postToAccount(
        playerID,
        [=](PlayerCreature&, Player& player) {
            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(&player);

            if (pGamePlayer == NULL)
                return;

            Assert(pGamePlayer->getPlayerStatus() == GPS_AFTER_SENDING_GL_INCOMING_CONNECTION);

            cout << "Fail to join game server...(" << playerID << ")" << endl;

            int fd = -1;
            Socket* pSocket = pGamePlayer->getSocket();
            if (pSocket != NULL)
                fd = (int)pSocket->getSOCKET();

            FILELOG_INCOMING_CONNECTION("incomingPenalty.log", "Error FD : %d, %s", fd,
                                        (pSocket == NULL ? "NULL" : pSocket->getHost().c_str()));

            pGamePlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
            pGamePlayer->setItemRatioBonusPoint(2);
        },
        nullptr, de::Scope::Player);

    if (!found)
        cout << "Player not exist or already disconnected." << endl;

#endif

    __END_DEBUG_EX
}
