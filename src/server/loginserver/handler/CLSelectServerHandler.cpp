//////////////////////////////////////////////////////////////////////////////
// Filename    : CLSelectServerHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLSelectServer.h"

#ifdef __LOGIN_SERVER__
#include <utility>

#include "Assert1.h"
#include "LCPCList.h"
#include "LoginCharacterList.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginWorldTopology.h"
#include "ServerContext.h"
#include "WorldSelection.h"
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

    SelectServerRequest request;
    request.worldID = pLoginPlayer->getWorldID();
    request.serverGroupID = pPacket->getServerGroupID();

    LoginWorldTopology topology(de::serverContext().worldInfos(), de::loginContext().gameServerGroups(),
                                de::loginContext().userInfos());

    Outcome<SelectedServer, SelectServerRejection> outcome = decideSelectServer(request, topology);

    if (outcome.isRejected()) {
        const auto& rejection = outcome.rejection();
        switch (rejection.reason) {
        case SelectServerReason::ServerClosed:
            filelog("errorLogin.txt", "Server Closed: %d", rejection.serverGroupID);
            throw DisconnectException("ServerClosed");
        case SelectServerReason::NoWorlds:
            filelog("errorLogin.txt", "No worlds configured");
            throw DisconnectException("NoWorlds");
        case SelectServerReason::NoServerGroups:
            filelog("errorLogin.txt", "No server groups configured for selected world");
            throw DisconnectException("NoServerGroups");
        }
    }

    const SelectedServer selected = std::move(outcome).events();

    pLoginPlayer->setServerGroupID(selected.serverGroupID);

    //----------------------------------------------------------------------
    // Answer with the account's characters.
    //----------------------------------------------------------------------
    auto lcPCList = de::makeLoginCharacterList(pLoginPlayer->getWorldID(), pLoginPlayer->getID(),
                                               defaultLoginCharacterRepository());

    pLoginPlayer->sendPacket(lcPCList.get());
    pLoginPlayer->setPlayerStatus(LPS_PC_MANAGEMENT);

    // The selected group is not written back here; CLChangeServerHandler
    // does that through LoginAccountRepository::setCurrentServerGroup.

#endif

    __END_DEBUG_EX __END_CATCH
}
