//////////////////////////////////////////////////////////////////////////////
// Filename    : CLQueryCharacterNameHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLQueryCharacterName.h"

#ifdef __LOGIN_SERVER__
#include "Assert.h"
#include "LoginCharacterNameQuery.h"
#include "LoginPlayer.h"
#include "ServerContext.h"
#include "repository/LoginCharacterRepository.h"
#endif


//////////////////////////////////////////////////////////////////////////////
// Check a character name in the selected world and reply with its availability.
//////////////////////////////////////////////////////////////////////////////
void CLQueryCharacterNameHandler::execute(CLQueryCharacterName* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    de::queryLoginCharacterName(*pLoginPlayer, *pPacket, de::serverContext().worldInfos(),
                                defaultLoginCharacterRepository(), de::defaultLoginCharacterNameQueryActions());

#endif

    __END_DEBUG_EX __END_CATCH
}
