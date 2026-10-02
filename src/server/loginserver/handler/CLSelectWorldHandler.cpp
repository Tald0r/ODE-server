//////////////////////////////////////////////////////////////////////////////
// Filename    : CLSelectWorldHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLSelectWorld.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginServerList.h"
#include "LoginWorldTopology.h"
#include "ServerContext.h"
#include "WorldSelection.h"
#include "repository/LoginAccountRepository.h"

#endif

//////////////////////////////////////////////////////////////////////////////
// World selection: put the session on the world the client picked and answer
// with that world's server groups.
//////////////////////////////////////////////////////////////////////////////
void CLSelectWorldHandler::execute(CLSelectWorld* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);
    WorldID_t WorldID = pPacket->getWorldID();

    LoginWorldTopology topology(de::serverContext().worldInfos(), de::loginContext().gameServerGroups(),
                                de::loginContext().userInfos());

    Outcome<void, SelectWorldRejection> outcome = decideSelectWorld(WorldID, topology);

    if (outcome.isRejected()) {
        const SelectWorldRejection& rejection = outcome.rejection();

        switch (rejection.reason) {
        case SelectWorldReason::UnknownWorld:
            filelog("errorLogin.txt", "WorldID Over[%d/%d]", (int)WorldID, rejection.worldCount);
            throw DisconnectException("WorldID over");

        case SelectWorldReason::WorldClosed:
            filelog("errorLogin.txt", "WorldClosed[%d]", (int)WorldID);
            throw DisconnectException("WorldClosed");
        }
    }

    pLoginPlayer->setWorldID(WorldID);

    try {
        de::sendLoginServerList(*pLoginPlayer, WorldID, topology, defaultLoginAccountRepository(),
                                de::LoginServerListQuery::CurrentGroup);
    } catch (Throwable&) {
        // A group the tables do not describe, or more groups than the list
        // packet holds, leaves the client without a server list.
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
