//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAppointSubmasterHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGAppointSubmaster.h"

#ifdef __GAME_SERVER__
#include "Assert1.h"
#include "GCGuildResponse.h"
#include "GCSystemMessage.h"
#include "GSModifyGuildMember.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "PlayerCreature.h"
#include "SharedServerManager.h"
#include "SystemAvailabilitiesManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGAppointSubmasterHandler::execute(CGAppointSubmaster* pPacket, Player* pPlayer)

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

    if (!de::gameContext().guilds().isGuildMaster(pPacket->getGuildID(), pPlayerCreature)) {
        // Send GC_GUILD_RESPONSE.
        // Content: not the guild master.
        GCGuildResponse gcGR;
        gcGR.setCode(GuildUnionOfferManager::SOURCE_IS_NOT_MASTER);
        pPlayer->sendPacket(&gcGR);
        return;
    }

    GSModifyGuildMember gsMGM;

    gsMGM.setGuildID(pPacket->getGuildID());
    gsMGM.setName(pPacket->getName());
    gsMGM.setGuildMemberRank(GuildMember::GUILDMEMBER_RANK_SUBMASTER);
    gsMGM.setSender(pPlayerCreature->getName());

    de::gameContext().sharedServer().sendPacket(&gsMGM);

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
