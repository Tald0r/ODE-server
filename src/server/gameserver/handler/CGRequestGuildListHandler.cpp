//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestGuildListHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRequestGuildList.h"

#ifdef __GAME_SERVER__
#include "Assert1.h"
#include "Creature.h"
#include "GCActiveGuildList.h"
#include "GCSystemMessage.h"
#include "GCWaitGuildList.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildInfo.h"
#include "GuildManager.h"
#include "NPC.h"
#include "Ousters.h"
#include "Slayer.h"
#include "SystemAvailabilitiesManager.h"
#include "Vampire.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRequestGuildListHandler::execute(CGRequestGuildList* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SYSTEM_ASSERT(SYSTEM_GUILD);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    GuildType_t tmpGuildType = pPacket->getGuildType();

    // If it is a waiting guild
    if (tmpGuildType == CGRequestGuildList::GUILDTYPE_WAIT) {
        GCWaitGuildList gcWaitGuildList;

        GuildRace_t race;
        if (pCreature->isSlayer())
            race = Guild::GUILD_RACE_SLAYER;
        else if (pCreature->isVampire())
            race = Guild::GUILD_RACE_VAMPIRE;
        else if (pCreature->isOusters())
            race = Guild::GUILD_RACE_OUSTERS;
        else
            return;

        de::gameContext().guilds().makeWaitGuildList(gcWaitGuildList, race);

        pPlayer->sendPacket(&gcWaitGuildList);

    }
    // If it is a normal guild
    else if (tmpGuildType == CGRequestGuildList::GUILDTYPE_NORMAL) {
        GCActiveGuildList gcActiveGuildList;

        GuildRace_t race;

        if (pCreature->isSlayer())
            race = Guild::GUILD_RACE_SLAYER;
        else if (pCreature->isVampire())
            race = Guild::GUILD_RACE_VAMPIRE;
        else if (pCreature->isOusters())
            race = Guild::GUILD_RACE_OUSTERS;
        else
            return;

        de::gameContext().guilds().makeActiveGuildList(gcActiveGuildList, race);

        pPlayer->sendPacket(&gcActiveGuildList);

    } else {
        return;
    }


#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
