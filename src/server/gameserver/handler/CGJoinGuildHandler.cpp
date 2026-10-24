//////////////////////////////////////////////////////////////////////////////
// Filename    : CGJoinGuildHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGJoinGuild.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "DB.h"
#include "GCNPCResponse.h"
#include "GSAddGuildMember.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "KernelContext.h"
#include "Ousters.h"
#include "Properties.h"
#include "SharedServerManager.h"
#include "Slayer.h"
#include "SystemAvailabilitiesManager.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "guild/GuildJoinDecision.h"
#include "repository/GuildRepository.h"

#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGJoinGuildHandler::execute(CGJoinGuild* pPacket, Player* pPlayer)

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

    Player* pPlayer = pCreature->getPlayer();
    Assert(pPlayer != NULL);

    // The dialogue that led here has already made these checks, so a refusal
    // means the client asked for something it was never offered. Nothing is
    // sent back.
    if (decideGuildJoinConfirm(defaultGuildRepository(), pCreature->getName(), time(0),
                               de::gameContext().variables().getVariable(QUIT_GUILD_PENALTY_TERM))
            .isRejected())
        return;

    if (pPacket->getGuildMemberRank() == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
        // A starting member may only join a guild that is still waiting for
        // approval.
        Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());
        if (pGuild == NULL)
            return;

        if (pGuild->getState() != Guild::GUILD_STATE_WAIT)
            return;

        GuildJoinStats stats;
        GuildJoinRequirements requirements;
        bool playableRace = false;

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            Assert(pSlayer != NULL);

            SkillDomainType_t highest = pSlayer->getHighestSkillDomain();

            playableRace = true;
            stats.level = pSlayer->getSkillDomainLevel(highest);
            stats.gold = pSlayer->getGold();
            stats.fame = pSlayer->getFame();
            requirements.level = REQUIRE_SLAYER_SUBMASTER_SKILL_DOMAIN_LEVEL;
            requirements.gold = REQUIRE_SLAYER_SUBMASTER_GOLD;
            requirements.fame = REQUIRE_SLAYER_SUBMASTER_FAME[highest];
            requirements.checkFame = true;
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            Assert(pVampire != NULL);

            playableRace = true;
            stats.level = pVampire->getLevel();
            stats.gold = pVampire->getGold();
            requirements.level = REQUIRE_VAMPIRE_SUBMASTER_LEVEL;
            requirements.gold = REQUIRE_VAMPIRE_SUBMASTER_GOLD;
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            Assert(pOusters != NULL);

            playableRace = true;
            stats.level = pOusters->getLevel();
            stats.gold = pOusters->getGold();
            requirements.level = REQUIRE_OUSTERS_SUBMASTER_LEVEL;
            requirements.gold = REQUIRE_OUSTERS_SUBMASTER_GOLD;
        }

        if (playableRace && decideGuildRequirements(GUILD_JOIN_CONTEXT_CONFIRM, stats, requirements).isOk()) {
            GSAddGuildMember gsAddGuildMember;

            gsAddGuildMember.setGuildID(pPacket->getGuildID());
            gsAddGuildMember.setName(pCreature->getName());
            gsAddGuildMember.setGuildMemberRank(pPacket->getGuildMemberRank());
            gsAddGuildMember.setGuildMemberIntro(pPacket->getGuildMemberIntro());
            gsAddGuildMember.setServerGroupID(de::kernelContext().config().getPropertyInt("ServerID"));

            de::gameContext().sharedServer().sendPacket(&gsAddGuildMember);
        }
    } else if (pPacket->getGuildMemberRank() == GuildMember::GUILDMEMBER_RANK_WAIT) {
        // An ordinary application, which waits for the guild to accept it.
        GSAddGuildMember gsAddGuildMember;

        gsAddGuildMember.setGuildID(pPacket->getGuildID());
        gsAddGuildMember.setName(pCreature->getName());
        gsAddGuildMember.setGuildMemberRank(pPacket->getGuildMemberRank());
        gsAddGuildMember.setGuildMemberIntro(pPacket->getGuildMemberIntro());
        gsAddGuildMember.setServerGroupID(de::kernelContext().config().getPropertyInt("ServerID"));

        de::gameContext().sharedServer().sendPacket(&gsAddGuildMember);
    }

    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPlayer->sendPacket(&response);

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
