//////////////////////////////////////////////////////////////////////////////
// Filename    : CLSelectServerHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLSelectServer.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginServerSelection.h"
#include "LoginWorldTopology.h"
#include "ServerContext.h"
#include "repository/LoginCharacterRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// The client picks a server group; the login server puts the session on it
// and answers with the account's characters.
//////////////////////////////////////////////////////////////////////////////
void CLSelectServerHandler::execute(CLSelectServer* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    LoginWorldTopology topology(de::serverContext().worldInfos(), de::loginContext().gameServerGroups(),
                                de::loginContext().userInfos());
    de::selectLoginServer(*pLoginPlayer, pPacket->getServerGroupID(), topology, defaultLoginCharacterRepository());

#endif

    __END_DEBUG_EX __END_CATCH
}
