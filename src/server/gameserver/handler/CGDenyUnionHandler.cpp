
//////////////////////////////////////////////////////////////////////////////
// Filename    : CGDenyUnionHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGDenyUnion.h"

#ifdef __GAME_SERVER__
#include "Assert1.h"
#include "DB.h"
#include "Exception.h"
#include "GCGuildResponse.h"
#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "PCFinder.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "StringPool.h"
#include "SystemAvailabilitiesManager.h"
#include "repository/MessageRepository.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGDenyUnionHandler::execute(CGDenyUnion* pPacket, Player* pPlayer)

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

    uint result = GuildUnionOfferManager::Instance().denyJoin(pPacket->getGuildID(), pUnion->getUnionID());

    gcGuildResponse.setCode(result);
    pPlayer->sendPacket(&gcGuildResponse);

    ////////////////////
    if (result == GuildUnionOfferManager::OK) {
        Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());

        if (pGuild == NULL) {
            return;
        }
        string TargetGuildMaster = pGuild->getMaster();

        defaultMessageRepository().insertUnionNotice(UNION_NOTICE_QUOTED_SPACED, TargetGuildMaster,
                                                     de::gameContext().strings().c_str(374));

        // A union the denied offer left with no member and no other pending
        // offer was dissolved by denyJoin, on every game server.

        Creature* pCreature = NULL;
        pCreature = pGamePlayer->getCreature();

        if (pCreature == NULL)
            return;

        GCModifyInformation gcModifyInformation;
        makeGCModifyInfoGuildUnion(&gcModifyInformation, pCreature);

        pPlayer->sendPacket(&gcModifyInformation);

        // Send the guild union information to the notified user again

        Creature* pTargetCreature = NULL;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        pTargetCreature = pcFinder.getCreature_LOCKED(TargetGuildMaster);
        if (pTargetCreature == NULL) {
            return;
        }
        GCModifyInformation gcModifyInformation2;
        makeGCModifyInfoGuildUnion(&gcModifyInformation2, pTargetCreature);
        pTargetCreature->getPlayer()->sendPacket(&gcModifyInformation2);

        __LEAVE_CRITICAL_SECTION(pcFinder)

        sendGCOtherModifyInfoGuildUnion(pTargetCreature);
        sendGCOtherModifyInfoGuildUnion(pCreature);

        // Tell the ones on other servers about the change.
        GuildUnionManager::Instance().sendModifyUnionInfo(dynamic_cast<PlayerCreature*>(pTargetCreature)->getGuildID());
        GuildUnionManager::Instance().sendModifyUnionInfo(dynamic_cast<PlayerCreature*>(pCreature)->getGuildID());
    }


#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
