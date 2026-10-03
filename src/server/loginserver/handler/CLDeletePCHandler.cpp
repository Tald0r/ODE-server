//////////////////////////////////////////////////////////////////////////////
// Filename    : CLDeletePCHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLDeletePC.h"

#ifdef __LOGIN_SERVER__
#include "Assert.h"
#include "LoginCharacterDeletion.h"
#include "LoginPlayer.h"
#include "repository/LoginCharacterPurgeRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CLDeletePCHandler::execute(CLDeletePC* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);
    (void)de::deleteLoginCharacter(*pLoginPlayer, *pPacket, defaultLoginCharacterPurgeRepository(),
                                   de::defaultLoginCharacterDeletionActions());

#endif

    __END_DEBUG_EX __END_CATCH
}
