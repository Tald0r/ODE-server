//----------------------------------------------------------------------
//
// Filename    : LGKickCharacterHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "LGKickCharacter.h"

#ifdef __GAME_SERVER__

#include "Creature.h"
#include "GLKickVerify.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "LogDef.h"
#include "LoginServerManager.h"
#include "PlayerMailbox.h"

#endif

//----------------------------------------------------------------------
//
// LGKickCharacterHander::execute()
//
// When the game server gets an LGKickCharacter packet from the login server,
// it adds a new ConnectionInfo.
//
//----------------------------------------------------------------------
void LGKickCharacterHandler::execute(LGKickCharacter* pPacket)

{
    __BEGIN_DEBUG_EX
#ifdef __GAME_SERVER__

    // hmm

    try {
        const string pcName = pPacket->getPCName();
        const string host = pPacket->getHost();
        const uint port = pPacket->getPort();
        const uint requestID = pPacket->getID();

        // With no such character, GLKickVerify(false) is sent.
        de::GoneCommand notHere = [=] {
            GLKickVerify glKickVerify;
            glKickVerify.setKicked(false);
            glKickVerify.setID(requestID);
            glKickVerify.setPCName(pcName);

            de::gameContext().loginServer().sendPacket(host, port, &glKickVerify);
        };

        // The kick flags are GamePlayer state, read by whichever manager
        // owns the player, so they are set on that thread (PlayerMailbox.h,
        // Scope::Player: the main thread applies it too while the player is
        // logging in or changing zone, which is exactly when a stuck session
        // needs kicking). A player that logs out between the post and the
        // owner's pass gets the same "not here" verify a player who was
        // never found gets, so the loginserver is answered either way; it
        // keys the reply on the request id and lets the pending login
        // through (GLKickVerifyHandler), which is what a completed kick
        // achieves as well.
        const bool found = de::postToPlayer(
            pcName,
            [=](PlayerCreature&, Player& player) {
                GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(&player);

                if (pGamePlayer == NULL) // how could that happen?
                {
                    return;
                }

                int fd = -1;
                Socket* pSocket = pGamePlayer->getSocket();
                if (pSocket != NULL)
                    fd = (int)pSocket->getSOCKET();

                FILELOG_INCOMING_CONNECTION("incomingDisconnect.log", "Kick FD : %d, %s", fd,
                                            (pSocket == NULL ? "NULL" : pSocket->getHost().c_str()));

                // Force the shutdown.
                pGamePlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                pGamePlayer->setItemRatioBonusPoint(4);
                pGamePlayer->setKickForLogin(true);

                // Where to send the answer after disconnecting..
                pGamePlayer->setKickRequestHost(host);
                pGamePlayer->setKickRequestPort(port);
            },
            notHere, de::Scope::Player);

        if (!found)
            notHere();
    } catch (NoSuchElementException&) {
    }

#endif

    __END_DEBUG_EX
}
