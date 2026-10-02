//////////////////////////////////////////////////////////////////////////////
// Filename    : CLGetServerListHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLGetServerList.h"

#ifdef __LOGIN_SERVER__
#include <vector>

#include "Assert1.h"
#include "LCServerList.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginWorldTopology.h"
#include "ServerContext.h"
#include "ServerGroupInfo.h"
#include "WorldSelection.h"
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

    // This list is built off the plain thresholds whatever the build.
    const ServerLoadThresholds thresholds;

    try {
        const std::vector<ServerListEntry> groups = serverListFor(WorldID, thresholds, topology);

        LCServerList lcServerList;

        // The account's current world and group. Both values land in the
        // server group field, the second overwriting the first.
        int currentWorldID = 0;
        int currentServerGroupID = 0;
        if (defaultLoginAccountRepository().loadCurrentLocation(LOGIN_LOCATION_SQL_LOWER, pLoginPlayer->getID(),
                                                                currentWorldID, currentServerGroupID)) {
            lcServerList.setCurrentServerGroupID(currentWorldID);
            lcServerList.setCurrentServerGroupID(currentServerGroupID);
        }

        for (std::vector<ServerListEntry>::const_iterator itr = groups.begin(); itr != groups.end(); ++itr) {
            ServerGroupInfo* pServerGroupInfo = new ServerGroupInfo();
            pServerGroupInfo->setGroupID(itr->groupID);
            pServerGroupInfo->setGroupName(itr->groupName);
            pServerGroupInfo->setStat(itr->stat);

            lcServerList.addListElement(pServerGroupInfo);
        }

        pLoginPlayer->sendPacket(&lcServerList);

        pLoginPlayer->setPlayerStatus(LPS_PC_MANAGEMENT);
    } catch (Throwable&) {
        // A group the tables do not describe, or more groups than the list
        // packet holds, leaves the client without a server list.
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
