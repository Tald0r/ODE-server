//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRegistGuildHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRegistGuild.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "DB.h"
#include "GCNPCResponse.h"
#include "GSAddGuild.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
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
void CGRegistGuildHandler::execute(CGRegistGuild* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SYSTEM_ASSERT(SYSTEM_GUILD);

    static_assert(GuildMember::GUILDMEMBER_RANK_LEAVE == kGuildMemberRankLeave);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    GuildRepository& guildRows = defaultGuildRepository();

    GuildRegistrationRequest request;
    request.name = pCreature->getName();
    request.guildName = pPacket->getGuildName();
    request.now = time(0);
    request.penaltyTermDays = de::gameContext().variables().getVariable(QUIT_GUILD_PENALTY_TERM);

    // The founding thresholds are the race's own: a Slayer is judged by the
    // level of its highest skill domain and by its fame, the other two races
    // by their level alone.
    GuildJoinRace race = GUILD_JOIN_RACE_SLAYER;
    GuildRace_t guildRace = Guild::GUILD_RACE_SLAYER;
    bool playableRace = false;
    GuildJoinStats stats;
    GuildJoinRequirements requirements;

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Assert(pSlayer != NULL);

        SkillDomainType_t highest = pSlayer->getHighestSkillDomain();

        race = GUILD_JOIN_RACE_SLAYER;
        guildRace = Guild::GUILD_RACE_SLAYER;
        playableRace = true;
        stats.level = pSlayer->getSkillDomainLevel(highest);
        stats.gold = pSlayer->getGold();
        stats.fame = pSlayer->getFame();
        requirements.level = REQUIRE_SLAYER_MASTER_SKILL_DOMAIN_LEVEL;
        requirements.gold = REQUIRE_SLAYER_MASTER_GOLD;
        requirements.fame = REQUIRE_SLAYER_MASTER_FAME[highest];
        requirements.checkFame = true;
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Assert(pVampire != NULL);

        race = GUILD_JOIN_RACE_VAMPIRE;
        guildRace = Guild::GUILD_RACE_VAMPIRE;
        playableRace = true;
        stats.level = pVampire->getLevel();
        stats.gold = pVampire->getGold();
        requirements.level = REQUIRE_VAMPIRE_MASTER_LEVEL;
        requirements.gold = REQUIRE_VAMPIRE_MASTER_GOLD;
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        Assert(pOusters != NULL);

        race = GUILD_JOIN_RACE_OUSTERS;
        guildRace = Guild::GUILD_RACE_OUSTERS;
        playableRace = true;
        stats.level = pOusters->getLevel();
        stats.gold = pOusters->getGold();
        requirements.level = REQUIRE_OUSTERS_MASTER_LEVEL;
        requirements.gold = REQUIRE_OUSTERS_MASTER_GOLD;
    }

    Outcome<GuildRegistrationClearance, GuildJoinRejection> eligibility = decideGuildRegistration(guildRows, request);

    if (eligibility.isRejected()) {
        uint16_t code = 0;
        if (guildJoinResponseCode(eligibility.rejection(), race, code)) {
            GCNPCResponse response;
            response.setCode(code);
            pPlayer->sendPacket(&response);
        }

        return;
    }

    // A membership row of a guild the character has already left is dropped
    // before the new guild is created.
    //
    // The unspaced spelling of the DELETE, which is this call site's.
    if (eligibility.events().clearStaleMemberRow)
        guildRows.deleteMemberSpelled(GUILD_MEMBER_DELETE_UNSPACED, pCreature->getName());

    // A founder who falls short is answered with silence: the NPC simply
    // does not create the guild.
    if (decideGuildRequirements(GUILD_JOIN_CONTEXT_REGIST, stats, requirements).isRejected())
        return;

    if (!playableRace)
        return;

    GSAddGuild gsAddGuild;

    gsAddGuild.setGuildName(pPacket->getGuildName());
    gsAddGuild.setGuildMaster(pCreature->getName());
    gsAddGuild.setGuildIntro(pPacket->getGuildIntro());
    gsAddGuild.setGuildState(Guild::GUILD_STATE_WAIT);
    gsAddGuild.setGuildRace(guildRace);
    gsAddGuild.setServerGroupID(de::kernelContext().config().getPropertyInt("ServerID"));

    de::gameContext().sharedServer().sendPacket(&gsAddGuild);

    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPlayer->sendPacket(&response);

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
