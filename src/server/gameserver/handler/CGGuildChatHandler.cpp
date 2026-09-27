//////////////////////////////////////////////////////////////////////////////
// Filename    : CGGuildChatHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGGuildChat.h"

#ifdef __GAME_SERVER__
#include "GGGuildChat.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "KernelContext.h"
#include "LoginServerManager.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "ServerContext.h"
#include "SystemAvailabilitiesManager.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGGuildChatHandler::execute(CGGuildChat* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SYSTEM_ASSERT(SYSTEM_GUILD);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    if (!pCreature->isPC())
        return;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    GGGuildChat ggGuildChat;
    ggGuildChat.setType(pPacket->getType());
    ggGuildChat.setGuildID(pPC->getGuildID());
    ggGuildChat.setSender(pPC->getName());
    ggGuildChat.setColor(pPacket->getColor());
    ggGuildChat.setMessage(pPacket->getMessage());

    // Send it to each server.
    HashMapGameServerInfo** pGameServerInfos = de::serverContext().serverInfos().getGameServerInfos();

    static int myWorldID = de::kernelContext().config().getPropertyInt("WorldID");
    [[maybe_unused]] static int myServerID = de::kernelContext().config().getPropertyInt("ServerID");

    int maxServerGroupID = de::serverContext().serverInfos().getMaxServerGroupID();

    for (int groupID = 0; groupID < maxServerGroupID; groupID++) {
        HashMapGameServerInfo& gameServerInfo = pGameServerInfos[myWorldID][groupID];

        if (!gameServerInfo.empty()) {
            HashMapGameServerInfo::const_iterator itr = gameServerInfo.begin();
            for (; itr != gameServerInfo.end(); itr++) {
                GameServerInfo* pGameServerInfo = itr->second;

                de::gameContext().loginServer().sendPacket(pGameServerInfo->getIP(), pGameServerInfo->getUDPPort(),
                                                           &ggGuildChat);
            }
        }
    }


#endif

    __END_DEBUG_EX __END_CATCH
}
