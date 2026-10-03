//////////////////////////////////////////////////////////////////////////////
// Filename    : CLCreatePCHandler.cc
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLCreatePC.h"

#ifdef __LOGIN_SERVER__
#include "Assert.h"
#include "CharacterCreation.h"
#include "LoginCharacterCreation.h"
#include "LoginPlayer.h"
#include "repository/LoginCharacterRepository.h"
#endif


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CLCreatePCHandler::execute(CLCreatePC* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);
    // The level-1 balance rows never change while the server runs, so one
    // cache serves every creation.
    static CreatePCBalanceCache balance;
    (void)de::createLoginCharacter(*pLoginPlayer, *pPacket, defaultLoginCharacterRepository(), balance,
                                   de::defaultLoginCharacterCreationActions());

#endif

    __END_DEBUG_EX __END_CATCH
}
