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
#include "LoginContext.h"
#include "LoginIncomingReply.h"
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

        (void)
            de::refuseLoginIncomingConnection(de::loginContext().loginPlayers(), *pPacket,
                                              std::chrono::steady_clock::now(), de::defaultLoginIncomingReplyActions());

#endif

    __END_DEBUG_EX __END_CATCH
}
