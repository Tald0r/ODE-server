////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionShowGuildDialog.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionShowGuildDialog.h"

#include "Creature.h"
#include "GCActiveGuildList.h"
#include "GCNPCResponse.h"
#include "GCWaitGuildList.h"
#include "GSQuitGuild.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildInfo.h"
#include "GuildManager.h"
#include "NPC.h"
#include "Ousters.h"
#include "SharedServerManager.h"
#include "Slayer.h"
#include "SystemAvailabilitiesManager.h"
#include "Vampire.h"
#include "repository/GuildRepository.h"

////////////////////////////////////////////////////////////////////////////////
// read from property buffer
////////////////////////////////////////////////////////////////////////////////
void ActionShowGuildDialog::read(PropertyBuffer& propertyBuffer)

{
    try {
        // Type of the dialog
        m_Type = (GuildDialog_t)propertyBuffer.getPropertyInt("Type");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionShowGuildDialog::execute(Creature* pCreature1, Creature* pCreature2)

{
    GuildManager& guilds = context().guilds();

    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_GUILD);

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    if (m_Type == GUILD_DIALOG_REGIST) {
        ////////////////////////////////////////////////////////////////////////////////
        // The player chose guild registration
        ////////////////////////////////////////////////////////////////////////////////
        // Check whether the player belongs to another guild
        int Rank = 0;
        string ExpireDate;

        {
            if (defaultGuildRepository().loadMemberRankExpireDate(pCreature->getName(), Rank, ExpireDate)) {
                // Guild registration info exists. Decide from the expire date.

                if (ExpireDate.size() == 7) {
                    // A player who left another guild cannot create a guild for a week.
                    if (Rank == GuildMember::GUILDMEMBER_RANK_LEAVE) {
                        // Not in a guild now, but 7 days must pass from the expire date.
                        time_t daytime = time(0);
                        tm Time;
                        Time.tm_year = atoi(ExpireDate.substr(0, 3).c_str());
                        Time.tm_mon = atoi(ExpireDate.substr(3, 2).c_str());
                        Time.tm_mday = atoi(ExpireDate.substr(5, 2).c_str());

                        if (difftime(daytime, mktime(&Time)) < 604800) // Have 7 real-time days passed?
                        {
                            // Less than a week since leaving the previous guild.
                            if (pCreature->isSlayer()) {
                                GCNPCResponse response;
                                response.setCode(NPC_RESPONSE_TEAM_REGIST_FAIL_QUIT_TIMEOUT);
                                pPlayer->sendPacket(&response);
                            } else if (pCreature->isVampire()) {
                                GCNPCResponse response;
                                response.setCode(NPC_RESPONSE_CLAN_REGIST_FAIL_QUIT_TIMEOUT);
                                pPlayer->sendPacket(&response);
                            } else if (pCreature->isOusters()) {
                                GCNPCResponse response;
                                response.setCode(NPC_RESPONSE_GUILD_REGIST_FAIL_QUIT_TIMEOUT);
                                pPlayer->sendPacket(&response);
                            }

                            return;
                        }
                    }
                } else {
                    // Already a member of another guild
                    if (pCreature->isSlayer()) {
                        GCNPCResponse response;
                        response.setCode(NPC_RESPONSE_TEAM_REGIST_FAIL_ALREADY_JOIN);
                        pPlayer->sendPacket(&response);
                    } else if (pCreature->isVampire()) {
                        GCNPCResponse response;
                        response.setCode(NPC_RESPONSE_CLAN_REGIST_FAIL_ALREADY_JOIN);
                        pPlayer->sendPacket(&response);
                    } else if (pCreature->isOusters()) {
                        GCNPCResponse response;
                        response.setCode(NPC_RESPONSE_GUILD_REGIST_FAIL_ALREADY_JOIN);
                        pPlayer->sendPacket(&response);
                    }


                    return;
                }
            }
        }

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            Assert(pSlayer != NULL);

            SkillDomainType_t highest = pSlayer->getHighestSkillDomain();

            if (pSlayer->getSkillDomainLevel(highest) < REQUIRE_SLAYER_MASTER_SKILL_DOMAIN_LEVEL) // Level 50 or higher
            {
                // Level too low
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_TEAM_REGIST_FAIL_LEVEL);
                pPlayer->sendPacket(&response);

                return;
            }
            if (pSlayer->getGold() < REQUIRE_SLAYER_MASTER_GOLD) // Registration fee
            {
                // Not enough money for the registration fee
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_TEAM_REGIST_FAIL_MONEY);
                pPlayer->sendPacket(&response);

                return;
            }
            if (pSlayer->getFame() < REQUIRE_SLAYER_MASTER_FAME[highest]) // Fame
            {
                // Not enough fame
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_TEAM_REGIST_FAIL_FAME);
                pPlayer->sendPacket(&response);

                return;
            }
            // Send the message that opens the guild registration window.
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_GUILD_SHOW_REGIST);
            response.setParameter(REQUIRE_SLAYER_MASTER_GOLD);
            pPlayer->sendPacket(&response);
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            Assert(pVampire != NULL);

            // Check whether registration is possible
            if (pVampire->getLevel() < REQUIRE_VAMPIRE_MASTER_LEVEL) // Level 50 or higher
            {
                // Level too low
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_CLAN_REGIST_FAIL_LEVEL);
                pPlayer->sendPacket(&response);

                return;
            }
            if (pVampire->getGold() < REQUIRE_VAMPIRE_MASTER_GOLD) // Registration fee
            {
                // Not enough money for the registration fee
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_CLAN_REGIST_FAIL_MONEY);
                pPlayer->sendPacket(&response);

                return;
            }
            //				// Not enough fame
            //
            // Send the message that opens the guild registration window.
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_GUILD_SHOW_REGIST);

            response.setParameter(REQUIRE_VAMPIRE_MASTER_GOLD);
            pPlayer->sendPacket(&response);
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            Assert(pOusters != NULL);

            // Check whether registration is possible
            if (pOusters->getLevel() < REQUIRE_OUSTERS_MASTER_LEVEL) // Level 50 or higher
            {
                // Level too low
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_GUILD_REGIST_FAIL_LEVEL);
                pPlayer->sendPacket(&response);

                return;
            }
            if (pOusters->getGold() < REQUIRE_OUSTERS_MASTER_GOLD) // Registration fee
            {
                // Not enough money for the registration fee
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_GUILD_REGIST_FAIL_MONEY);
                pPlayer->sendPacket(&response);

                return;
            }

            // Send the message that opens the guild registration window.
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_GUILD_SHOW_REGIST);

            response.setParameter(REQUIRE_OUSTERS_MASTER_GOLD);
            pPlayer->sendPacket(&response);
        }
    } else if (m_Type == GUILD_DIALOG_WAIT_LIST) {
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

        guilds.makeWaitGuildList(gcWaitGuildList, race);

        pPlayer->sendPacket(&gcWaitGuildList);
    } else if (m_Type == GUILD_DIALOG_LIST) {
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

        guilds.makeActiveGuildList(gcActiveGuildList, race);

        pPlayer->sendPacket(&gcActiveGuildList);
    } else if (m_Type == GUILD_DIALOG_QUIT) {
        // Treat the leave as confirmed without opening the guild leave window.
        Guild* pGuild = NULL;

        int guildID = 0;
        if (defaultGuildRepository().loadMemberGuildID(pCreature->getName(), guildID)) {
            pGuild = guilds.getGuild(guildID);
        }

        // The guild state must be active or waiting.
        if (pGuild == NULL ||
            (pGuild->getState() != Guild::GUILD_STATE_ACTIVE && pGuild->getState() != Guild::GUILD_STATE_WAIT)) {
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&response);

            return;
        }

        // Check that the player is a member of the guild. A member the guild
        // let go is retired rather than freed, so the pointer stays readable
        // and says so.
        GuildMember* pGuildMember = pGuild->getMember(pCreature->getName());
        if (pGuildMember == NULL || pGuildMember->isRetired()) {
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&response);

            return;
        }

        // Ignore the leave if the player is the master of an active guild.
        if (pGuildMember->getRank() == GuildMember::GUILDMEMBER_RANK_MASTER &&
            pGuild->getState() == Guild::GUILD_STATE_ACTIVE) {
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&response);

            return;
        }

        GSQuitGuild gsQuitGuild;
        gsQuitGuild.setGuildID(pGuild->getID());
        gsQuitGuild.setName(pCreature->getName());

        context().sharedServer().sendPacket(&gsQuitGuild);

        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&response);
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionShowGuildDialog::toString() const

{
    string str = "ActionShowGuildDialog";

    return str;
}
