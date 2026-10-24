//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTryJoinGuildHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGTryJoinGuild.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "Assert1.h"
#include "DB.h"
#include "GCNPCResponse.h"
#include "GCShowGuildJoin.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Ousters.h"
#include "Slayer.h"
#include "StringPool.h"
#include "SystemAvailabilitiesManager.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "guild/GuildJoinDecision.h"
#include "repository/GuildRepository.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTryJoinGuildHandler::execute(CGTryJoinGuild* pPacket, Player* pPlayer)

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

    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());

    GuildJoinAttempt attempt;
    attempt.name = pCreature->getName();
    attempt.guildExists = (pGuild != NULL);
    attempt.now = time(0);
    attempt.penaltyTermDays = de::gameContext().variables().getVariable(QUIT_GUILD_PENALTY_TERM);
    attempt.waitMemberLimit = MAX_GUILDMEMBER_WAIT_COUNT;
    if (pGuild != NULL)
        attempt.waitMemberCount = pGuild->getWaitMemberCount();

    if (pPacket->getGuildMemberRank() == GuildMember::GUILDMEMBER_RANK_SUBMASTER)
        attempt.rank = GUILD_JOIN_RANK_STARTING;
    else if (pPacket->getGuildMemberRank() == GuildMember::GUILDMEMBER_RANK_WAIT)
        attempt.rank = GUILD_JOIN_RANK_WAITING;

    // The starting-member thresholds are the race's own: a Slayer is judged
    // by the level of its highest skill domain and by its fame, the other two
    // races by their level alone.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Assert(pSlayer != NULL);

        SkillDomainType_t highest = pSlayer->getHighestSkillDomain();

        attempt.race = GUILD_JOIN_RACE_SLAYER;
        attempt.stats.level = pSlayer->getSkillDomainLevel(highest);
        attempt.stats.gold = pSlayer->getGold();
        attempt.stats.fame = pSlayer->getFame();
        attempt.requirements.level = REQUIRE_SLAYER_SUBMASTER_SKILL_DOMAIN_LEVEL;
        attempt.requirements.gold = REQUIRE_SLAYER_SUBMASTER_GOLD;
        attempt.requirements.fame = REQUIRE_SLAYER_SUBMASTER_FAME[highest];
        attempt.requirements.checkFame = true;
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Assert(pVampire != NULL);

        attempt.race = GUILD_JOIN_RACE_VAMPIRE;
        attempt.stats.level = pVampire->getLevel();
        attempt.stats.gold = pVampire->getGold();
        attempt.requirements.level = REQUIRE_VAMPIRE_SUBMASTER_LEVEL;
        attempt.requirements.gold = REQUIRE_VAMPIRE_SUBMASTER_GOLD;
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        Assert(pOusters != NULL);

        attempt.race = GUILD_JOIN_RACE_OUSTERS;
        attempt.stats.level = pOusters->getLevel();
        attempt.stats.gold = pOusters->getGold();
        attempt.requirements.level = REQUIRE_OUSTERS_SUBMASTER_LEVEL;
        attempt.requirements.gold = REQUIRE_OUSTERS_SUBMASTER_GOLD;
    }

    Outcome<void, GuildJoinRejection> decision = decideGuildJoinAttempt(defaultGuildRepository(), attempt);

    if (decision.isRejected()) {
        const GuildJoinRejection& rejection = decision.rejection();

        uint16_t code = 0;
        if (guildJoinResponseCode(rejection, attempt.race, code)) {
            GCNPCResponse response;
            response.setCode(code);
            pPlayer->sendPacket(&response);
        }

        // A full waiting list is the one refusal that also says why.
        if (rejection.reason == GUILD_JOIN_REJECT_WAIT_LIST_FULL) {
            GCSystemMessage msg;
            msg.setMessage(de::gameContext().strings().getString(STRID_GUILD_WAIT_MEMBER_FULL));
            pPlayer->sendPacket(&msg);
        }

        return;
    }

    // Open the join dialogue. A starting member is quoted the fee its race
    // pays; an ordinary applicant pays nothing.
    if (attempt.rank == GUILD_JOIN_RANK_STARTING || attempt.rank == GUILD_JOIN_RANK_WAITING) {
        GCShowGuildJoin gcShowGuildJoin;
        gcShowGuildJoin.setGuildID(pGuild->getID());
        gcShowGuildJoin.setGuildName(pGuild->getName());
        gcShowGuildJoin.setGuildMemberRank(pPacket->getGuildMemberRank());
        gcShowGuildJoin.setJoinFee(
            attempt.rank == GUILD_JOIN_RANK_STARTING ? static_cast<Gold_t>(attempt.requirements.gold) : 0);
        pPlayer->sendPacket(&gcShowGuildJoin);
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
