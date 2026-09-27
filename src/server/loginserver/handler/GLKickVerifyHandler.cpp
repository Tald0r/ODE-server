//----------------------------------------------------------------------
//
// Filename    : GLKickVerifyHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GLKickVerify.h"

#ifdef __LOGIN_SERVER__

#include "LCLoginOK.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginPlayerManager.h"

#endif


//----------------------------------------------------------------------
//
// GLKickVerifyHander::execute()
//
// When the game server receives a GLKickVerify packet from the login server,
// a new ReconnectLoginInfo is added.
//
//----------------------------------------------------------------------
void GLKickVerifyHandler::execute(GLKickVerify* pPacket)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX __BEGIN_DEBUG
#ifdef __LOGIN_SERVER__


        LoginPlayerManager& loginPlayers = de::loginContext().loginPlayers();

    try {
        loginPlayers.lock();

        // The PlayerManager overload, keyed by socket.
        Player* pPlayer = static_cast<PlayerManager&>(loginPlayers).getPlayer(pPacket->getID());
        LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

        if (pLoginPlayer != NULL) // not strictly needed since NoSuch is used..
        {
            // The character names must match.
            const string& name1 = pLoginPlayer->getLastCharacterName();
            const string& name2 = pPacket->getPCName();

            if (name1.size() != 0 && name2.size() != 0 && name1 == name2) {
                pLoginPlayer->sendLCLoginOK();
            } else {
                // A different person. Nothing to worry about.
            }
        }

        loginPlayers.unlock();
    } catch (Throwable&) { // (NoSuchException&) { // would be pointless.
        loginPlayers.unlock();
    }

#endif

    __END_DEBUG
    __END_DEBUG_EX __END_CATCH
}
