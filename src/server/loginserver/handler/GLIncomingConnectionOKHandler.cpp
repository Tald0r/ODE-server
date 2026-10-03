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

#include "LoginContext.h"
#include "LoginIncomingReply.h"

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

        (void) de::completeLoginIncomingConnection(de::loginContext().loginPlayers(), *pPacket,
                                                   std::chrono::steady_clock::now(),
                                                   de::defaultLoginIncomingReplyActions());

#endif

    __END_DEBUG_EX __END_CATCH
}
