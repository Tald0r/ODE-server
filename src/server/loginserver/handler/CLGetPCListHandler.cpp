//////////////////////////////////////////////////////////////////////////////
// Filename    : CLGetPCListHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLGetPCList.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "LoginCharacterListRefresh.h"
#include "LoginPlayer.h"
#include "repository/LoginCharacterRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// When a client asks for the list of PCs, the login server loads the PCs'
// information from the DB and sends it in an LCPCList packet.
//////////////////////////////////////////////////////////////////////////////
void CLGetPCListHandler::execute(CLGetPCList* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    de::refreshLoginCharacterList(*pLoginPlayer, defaultLoginCharacterRepository(),
                                  de::defaultLoginCharacterListRefreshActions());

#endif

    __END_DEBUG_EX __END_CATCH
}
