
//////////////////////////////////////////////////////////////////////////////
// Filename    : CGQuitUnionDenyHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGQuitUnionDeny.h"

#ifdef __GAME_SERVER__
#include "Assert.h"
#include "DB.h"
#include "GCGuildResponse.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "PlayerCreature.h"
#include "StringPool.h"
#include "SystemAvailabilitiesManager.h"
#include "repository/MessageRepository.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGQuitUnionDenyHandler::execute(CGQuitUnionDeny* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPlayerCreature != NULL);

    SYSTEM_ASSERT(SYSTEM_GUILD);

    GCGuildResponse gcGuildResponse;

    GuildUnion* pUnion = GuildUnionManager::Instance().getGuildUnion(pPlayerCreature->getGuildID());
    if (pUnion == NULL) {
        gcGuildResponse.setCode(GuildUnionOfferManager::NOT_IN_UNION);
        pPlayer->sendPacket(&gcGuildResponse);

        return;
    }

    // Is the requester the master of its own guild, and is the union's master guild my guild?
    if (!de::gameContext().guilds().isGuildMaster(pPlayerCreature->getGuildID(), pPlayerCreature) ||
        pUnion->getMasterGuildID() != pPlayerCreature->getGuildID()) {
        // Send GC_GUILD_RESPONSE.
        // Content: not the guild master.

        gcGuildResponse.setCode(GuildUnionOfferManager::SOURCE_IS_NOT_MASTER);
        pPlayer->sendPacket(&gcGuildResponse);

        return;
    }

    uint result = GuildUnionOfferManager::Instance().denyQuit(pPacket->getGuildID(), pUnion->getUnionID());

    gcGuildResponse.setCode(result);
    pPlayer->sendPacket(&gcGuildResponse);

    // Only a denied offer is news to the guild that made it.
    if (result != GuildUnionOfferManager::OK)
        return;

    ////////////////////

    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());

    if (pGuild == NULL) {
        return;
    }
    string TargetGuildMaster = pGuild->getMaster();


    defaultMessageRepository().insertUnionNotice(UNION_NOTICE_PLAIN, TargetGuildMaster,
                                                 de::gameContext().strings().c_str(376));

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
