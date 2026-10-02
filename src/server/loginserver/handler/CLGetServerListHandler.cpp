//////////////////////////////////////////////////////////////////////////////
// Filename    : CLGetServerListHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLGetServerList.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginServerList.h"
#include "LoginWorldTopology.h"
#include "ServerContext.h"
#include "repository/LoginAccountRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// The client asks for the server list; the login server answers with the
// groups of the world the session is on.
//////////////////////////////////////////////////////////////////////////////
void CLGetServerListHandler::execute(CLGetServerList* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    WorldID_t WorldID = pLoginPlayer->getWorldID();

    LoginWorldTopology topology(de::serverContext().worldInfos(), de::loginContext().gameServerGroups(),
                                de::loginContext().userInfos());

    try {
        de::sendLoginServerList(*pLoginPlayer, WorldID, topology, defaultLoginAccountRepository(),
                                de::LoginServerListQuery::CurrentLocation);

        pLoginPlayer->setPlayerStatus(LPS_PC_MANAGEMENT);
    } catch (Throwable&) {
        // A group the tables do not describe, or more groups than the list
        // packet holds, leaves the client without a server list.
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
