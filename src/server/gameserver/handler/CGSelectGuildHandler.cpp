//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSelectGuildHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSelectGuild.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "GCShowGuildInfo.h"
#include "GCShowWaitGuildInfo.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Player.h"
#include "SystemAvailabilitiesManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSelectGuildHandler::execute(CGSelectGuild* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SYSTEM_ASSERT(SYSTEM_GUILD);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = dynamic_cast<Creature*>(pGamePlayer->getCreature());

    // Get the selected guild.
    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());


    if (pGuild == NULL) {
        return;
    }

    if (pGuild->getState() == Guild::GUILD_STATE_ACTIVE) {
        GCShowGuildInfo gcShowGuildInfo;
        gcShowGuildInfo.setGuildID(pGuild->getID());
        gcShowGuildInfo.setGuildName(pGuild->getName());
        gcShowGuildInfo.setGuildState(pGuild->getState());
        gcShowGuildInfo.setGuildMaster(pGuild->getMaster());
        gcShowGuildInfo.setGuildMemberCount(pGuild->getActiveMemberCount());
        gcShowGuildInfo.setGuildIntro(pGuild->getIntro());
        gcShowGuildInfo.setJoinFee(0);

        pPlayer->sendPacket(&gcShowGuildInfo);
    } else if (pGuild->getState() == Guild::GUILD_STATE_WAIT) {
        GCShowWaitGuildInfo gcShowWaitGuildInfo;
        gcShowWaitGuildInfo.setGuildID(pGuild->getID());
        gcShowWaitGuildInfo.setGuildName(pGuild->getName());
        gcShowWaitGuildInfo.setGuildState(pGuild->getState());
        gcShowWaitGuildInfo.setGuildMaster(pGuild->getMaster());
        gcShowWaitGuildInfo.setGuildMemberCount(pGuild->getActiveMemberCount());
        gcShowWaitGuildInfo.setGuildIntro(pGuild->getIntro());

        if (pCreature->isSlayer())
            gcShowWaitGuildInfo.setJoinFee(REQUIRE_SLAYER_SUBMASTER_GOLD);
        else if (pCreature->isVampire())
            gcShowWaitGuildInfo.setJoinFee(REQUIRE_VAMPIRE_SUBMASTER_GOLD);
        else if (pCreature->isOusters())
            gcShowWaitGuildInfo.setJoinFee(REQUIRE_OUSTERS_SUBMASTER_GOLD);

        // A copy under the guild mutex: this is a zone thread, and the
        // SharedServerManager thread adds and removes members meanwhile.
        const std::vector<std::string> names = pGuild->getMemberNames();
        for (size_t i = 0; i < names.size(); i++)
            gcShowWaitGuildInfo.addMember(names[i]);

        pPlayer->sendPacket(&gcShowWaitGuildInfo);
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
