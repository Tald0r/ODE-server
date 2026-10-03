//--------------------------------------------------------------------------------
//
// Filename    : GLIncomingConnectionErrorHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "GLIncomingConnectionError.h"

#ifdef __LOGIN_SERVER__
#include <mutex>

#include "Assert1.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginPlayerManager.h"
#endif

//--------------------------------------------------------------------------------
//
// GLIncomingConnectionErrorHander::execute()
//
//--------------------------------------------------------------------------------
void GLIncomingConnectionErrorHandler::execute(GLIncomingConnectionError* pPacket)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX
#ifdef __LOGIN_SERVER__

        try {
        LoginPlayerManager& loginPlayers = de::loginContext().loginPlayers();
        std::lock_guard guard(loginPlayers);
        LoginPlayer* pLoginPlayer = loginPlayers.getPlayer_NOLOCKED(pPacket->getPlayerID());

        Assert(pLoginPlayer->getPlayerStatus() == LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);

        loginPlayers.retirePlayer_NOLOCKED(pLoginPlayer->getSocket()->getSOCKET(), UNDISCONNECTED);
    } catch (NoSuchElementException& nsee) {
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
