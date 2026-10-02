//////////////////////////////////////////////////////////////////////////////
// Filename    : CLGetWorldListHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLGetWorldList.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "LoginPlayer.h"
#include "LoginWorldList.h"
#include "ServerContext.h"
#include "repository/LoginAccountRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// When a client asks for the list of servers, the login server loads the
// servers' information from the DB and sends it in an LCWorldList packet.
//////////////////////////////////////////////////////////////////////////////
void CLGetWorldListHandler::execute(CLGetWorldList* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    try {
        de::sendLoginWorldList(*pLoginPlayer, de::serverContext().worldInfos(), defaultLoginAccountRepository());
    } catch (Throwable& t) {
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
