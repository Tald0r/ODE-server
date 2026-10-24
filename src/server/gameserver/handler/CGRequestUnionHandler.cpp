//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestUnionHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRequestUnion.h"

#ifdef __GAME_SERVER__
#include "Assert1.h"
#include "Exception.h"
#include "GCGuildResponse.h"
#include "GCModifyInformation.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "PCFinder.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "SystemAvailabilitiesManager.h"

#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRequestUnionHandler::execute(CGRequestUnion* pPacket, Player* pPlayer)

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

    // Is the requester the master of the guild it belongs to?
    if (!de::gameContext().guilds().isGuildMaster(pPlayerCreature->getGuildID(), pPlayerCreature)) {
        // Send GC_GUILD_RESPONSE.
        // Content: not the guild master.

        gcGuildResponse.setCode(GuildUnionOfferManager::SOURCE_IS_NOT_MASTER);
        pPlayer->sendPacket(&gcGuildResponse);
        return;
    }

    uint result = GuildUnionOfferManager::Instance().offerJoin(pPlayerCreature->getGuildID(), pPacket->getGuildID());

    gcGuildResponse.setCode(result);
    pPlayer->sendPacket(&gcGuildResponse);

    if (result == GuildUnionOfferManager::OK) {
        // The other guild's master name has to be found.
        Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());

        if (pGuild != NULL) {
            string targetGuildMaster = pGuild->getMaster();

            PCFinder& pcFinder = de::gameContext().playerCreatures();

            __ENTER_CRITICAL_SECTION(pcFinder)

            Creature* pCreature = pcFinder.getCreature_LOCKED(targetGuildMaster);
            if (pCreature != NULL) {
                GCModifyInformation gcModifyInformation;
                makeGCModifyInfoGuildUnion(&gcModifyInformation, pCreature);
                pCreature->getPlayer()->sendPacket(&gcModifyInformation);
            }

            __LEAVE_CRITICAL_SECTION(pcFinder)
        }
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
