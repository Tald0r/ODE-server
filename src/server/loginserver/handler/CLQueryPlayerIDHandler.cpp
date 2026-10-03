//////////////////////////////////////////////////////////////////////////////
// Filename    : CLQueryPlayerIDHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLQueryPlayerID.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "LoginAccountNameQuery.h"
#include "LoginPlayer.h"
#include "repository/LoginAccountRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// Look a given player id up in the DB and tell the client whether it exists.
//////////////////////////////////////////////////////////////////////////////
void CLQueryPlayerIDHandler::execute(CLQueryPlayerID* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    de::queryLoginAccountName(*pLoginPlayer, *pPacket, defaultLoginAccountRepository(),
                              de::defaultLoginAccountNameQueryActions());

#endif

    __END_DEBUG_EX __END_CATCH
}
