//----------------------------------------------------------------------
//
// Filename    : SGExpelGuildMemberOKHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameContext.h"
#include "Properties.h"
#include "SGExpelGuildMemberOK.h"

#ifdef __GAME_SERVER__

#include <stdio.h>

#include "GCModifyGuildMemberInfo.h"
#include "GCOtherModifyInfo.h"
#include "GCSystemMessage.h"
#include "Guild.h"
#include "GuildManager.h"
#include "PCFinder.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "PlayerMailbox.h"
#include "StringPool.h"
#include "Zone.h"

#endif

//----------------------------------------------------------------------
//
// SGExpelGuildMemberOKHandler::execute()
//
//----------------------------------------------------------------------
void SGExpelGuildMemberOKHandler::execute(SGExpelGuildMemberOK* pPacket)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

        StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__


    Assert(pPacket != NULL);

    // Get the guild.
    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());
    try {
        Assert(pGuild != NULL);
    } catch (Throwable&) {
        return;
    }

    // Check whether it is a guild member.
    GuildMember* pGuildMember = pGuild->getMember(pPacket->getName());
    try {
        Assert(pGuildMember != NULL);
    } catch (Throwable&) {
        return;
    }

    if (pGuildMember->getRank() == GuildMember::GUILDMEMBER_RANK_WAIT) {
        //////////////////////////////////////////////////////////
        // Cancel the join
        //////////////////////////////////////////////////////////

        // Delete from the guild.
        pGuild->deleteMember(pGuildMember->getName());

        // Send a message if connected.
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pCreature = pcFinder.getCreature_LOCKED(pPacket->getName());
        if (pCreature != NULL && pCreature->isPC()) {
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);


            char msg[100];
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_TEAM_JOIN_DENY), pGuild->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_JOIN_DENY), pGuild->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_JOIN_DENY), pGuild->getName().c_str());
            // Send the guild join cancellation message.
            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(msg);
            pPlayer->sendPacket(&gcSystemMessage);
        }

        // Send the one who cancelled a message.
        pCreature = pcFinder.getCreature_LOCKED(pPacket->getSender());
        if (pCreature != NULL && pCreature->isPC()) {
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);


            char msg[100];
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_TEAM_JOIN_DENY_2), pPacket->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_JOIN_DENY_2), pPacket->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_JOIN_DENY_2), pPacket->getName().c_str());

            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(msg);
            pPlayer->sendPacket(&gcSystemMessage);
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    } else {
        ///////////////////////////////////////////////////////////
        // Expel from the guild.
        ///////////////////////////////////////////////////////////

        // Delete from the guild.
        pGuild->deleteMember(pGuildMember->getName());

        // If the expelled member is online, reset its guild id and tell the
        // zone -- creature and zone state, so on the owning zone thread
        // (PlayerMailbox.h) with the guild facts captured by value.
        {
            const GuildRace_t guildRace = pGuild->getRace();
            const GuildState_t guildState = pGuild->getState();
            de::postToPlayer(pPacket->getName(), [=](PlayerCreature& pc, Player& player) {
                if (pc.isSlayer()) {
                    pc.setGuildID(99); // the guild ID of a Slayer that has not joined

                    // Tell the client about the guild expulsion.
                    GCModifyGuildMemberInfo gcModifyGuildMember;
                    gcModifyGuildMember.setGuildID(pc.getGuildID());
                    gcModifyGuildMember.setGuildName("");
                    gcModifyGuildMember.setGuildMemberRank(GuildMember::GUILDMEMBER_RANK_DENY);
                    player.sendPacket(&gcModifyGuildMember);
                } else if (pc.isVampire()) {
                    pc.setGuildID(0); // the guild ID of a Vampire that has not joined

                    // Tell the client about the guild expulsion.
                    GCModifyGuildMemberInfo gcModifyGuildMember;
                    gcModifyGuildMember.setGuildID(pc.getGuildID());
                    gcModifyGuildMember.setGuildName("");
                    gcModifyGuildMember.setGuildMemberRank(GuildMember::GUILDMEMBER_RANK_DENY);
                    player.sendPacket(&gcModifyGuildMember);
                } else if (pc.isOusters()) {
                    pc.setGuildID(66); // the guild ID of an Ousters that has not joined

                    // Tell the client about the guild expulsion.
                    GCModifyGuildMemberInfo gcModifyGuildMember;
                    gcModifyGuildMember.setGuildID(pc.getGuildID());
                    gcModifyGuildMember.setGuildName("");
                    gcModifyGuildMember.setGuildMemberRank(GuildMember::GUILDMEMBER_RANK_DENY);
                    player.sendPacket(&gcModifyGuildMember);
                }

                // Send the guild expulsion message.
                GCSystemMessage gcSystemMessage;

                if (guildRace == Guild::GUILD_RACE_SLAYER)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_EXPEL_TEAM_MEMBER));
                else if (guildRace == Guild::GUILD_RACE_VAMPIRE)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_EXPEL_CLAN_MEMBER));
                else if (guildRace == Guild::GUILD_RACE_OUSTERS)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_EXPEL_CLAN_MEMBER));

                player.sendPacket(&gcSystemMessage);

                if (guildState == Guild::GUILD_STATE_ACTIVE) {
                    // Tell those around.
                    Zone* pZone = pc.getZone();
                    Assert(pZone != NULL);

                    GCOtherModifyInfo gcOtherModifyInfo;
                    gcOtherModifyInfo.setObjectID(pc.getObjectID());
                    gcOtherModifyInfo.addShortData(MODIFY_GUILDID, pc.getGuildID());

                    pZone->broadcastPacket(pc.getX(), pc.getY(), &gcOtherModifyInfo, &pc);
                }
            });
        }

        // Send the one who expelled a message. (send only: fine from this thread)
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pCreature = pcFinder.getCreature_LOCKED(pPacket->getSender());
        if (pCreature != NULL && pCreature->isPC()) {
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);


            char msg[100];
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_EXPEL_TEAM_MEMBER_2), pPacket->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_EXPEL_CLAN_MEMBER_2), pPacket->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_EXPEL_CLAN_MEMBER_2), pPacket->getName().c_str());

            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(msg);
            pPlayer->sendPacket(&gcSystemMessage);
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
