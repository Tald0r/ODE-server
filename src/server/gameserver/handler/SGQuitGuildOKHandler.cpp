//----------------------------------------------------------------------
//
// Filename    : SGQuitGuildOKHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameContext.h"
#include "Properties.h"
#include "SGQuitGuildOK.h"

#ifdef __GAME_SERVER__

#include <stdio.h>

#include "GCModifyGuildMemberInfo.h"
#include "GCModifyInformation.h"
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
// SGQuitGuildOKHandler::execute()
//
//----------------------------------------------------------------------
void SGQuitGuildOKHandler::execute(SGQuitGuildOK* pPacket)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

        StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);

    // Get the guild.
    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());
    // try { Assert(pGuild != NULL); } catch (Throwable& ) { return; }
    if (pGuild == NULL)
        return;

    // Check whether it is a guild member.
    GuildMember* pGuildMember = pGuild->getMember(pPacket->getName());
    // try { Assert(pGuildMember != NULL); } catch (Throwable& ) { return; }
    if (pGuildMember == NULL)
        return;

    string memberName = pGuildMember->getName();

    // If the member is online, reset its guild id / refund the fee and tell
    // the zone. That mutates creature and zone state, so it runs on the
    // owning zone thread (PlayerMailbox.h) with everything captured by
    // value: pGuildMember is deleted below, before the command can run.
    {
        const GuildState_t guildState = pGuild->getState();
        const GuildID_t guildID = pGuild->getID();
        const string guildName = pGuild->getName();
        const GuildRace_t guildRace = pGuild->getRace();
        const GuildMemberRank_t rank = pGuildMember->getRank();
        de::postToPlayer(memberName, [=](PlayerCreature& pc, Player& player) {
            if (guildState == Guild::GUILD_STATE_ACTIVE) {
                ////////////////////////////////////////////////////////////////////////////////
                // For a guild that was active, set the Slayer's and Vampire's guild id back to not joined.
                ////////////////////////////////////////////////////////////////////////////////
                if (pc.isSlayer()) {
                    pc.setGuildID(99); // the guild ID of a Slayer that has not joined

                    // Send the message to the client.
                    GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
                    gcModifyGuildMemberInfo.setGuildID(guildID);
                    gcModifyGuildMemberInfo.setGuildName(guildName);
                    gcModifyGuildMemberInfo.setGuildMemberRank(rank);
                    player.sendPacket(&gcModifyGuildMemberInfo);
                } else if (pc.isVampire()) {
                    pc.setGuildID(0); // the guild ID of a Vampire that has not joined

                    // Send the message to the client.
                    GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
                    gcModifyGuildMemberInfo.setGuildID(guildID);
                    gcModifyGuildMemberInfo.setGuildName(guildName);
                    gcModifyGuildMemberInfo.setGuildMemberRank(rank);
                    player.sendPacket(&gcModifyGuildMemberInfo);
                } else if (pc.isOusters()) {
                    pc.setGuildID(66); // the guild ID of an Ousters that has not joined

                    // Send the message to the client.
                    GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
                    gcModifyGuildMemberInfo.setGuildID(guildID);
                    gcModifyGuildMemberInfo.setGuildName(guildName);
                    gcModifyGuildMemberInfo.setGuildMemberRank(rank);
                    player.sendPacket(&gcModifyGuildMemberInfo);
                }
            }

            if (guildState == Guild::GUILD_STATE_WAIT && rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
                ///////////////////////////////////////////////////////////
                // For a submaster of a waiting guild, refund the registration fee.
                ///////////////////////////////////////////////////////////
                Gold_t Gold = pc.getGold();
                Gold = min((uint64_t)(Gold + RETURN_SLAYER_SUBMASTER_GOLD), (uint64_t)2000000000);
                pc.setGoldEx(Gold);

                GCModifyInformation gcModifyInformation;
                gcModifyInformation.addLongData(MODIFY_GOLD, Gold);
                player.sendPacket(&gcModifyInformation);
            }

            // Send the guild withdrawal message.
            GCSystemMessage gcSystemMessage;
            if (guildRace == Guild::GUILD_RACE_SLAYER)
                gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_QUIT_TEAM));
            else if (guildRace == Guild::GUILD_RACE_VAMPIRE)
                gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_QUIT_CLAN));
            else if (guildRace == Guild::GUILD_RACE_OUSTERS)
                gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_QUIT_CLAN));
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

    // Delete from the guild.
    pGuild->deleteMember(memberName);

    // Send the guild master a message. (send only: fine from this thread)
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    Creature* pCreature = pcFinder.getCreature_LOCKED(pGuild->getMaster());
    if (pCreature != NULL && pCreature->isPC()) {
        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer != NULL);


        char msg[100];
        if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
            snprintf(msg, sizeof(msg), strings.c_str(STRID_QUIT_TEAM_2), memberName.c_str());
        else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
            snprintf(msg, sizeof(msg), strings.c_str(STRID_QUIT_CLAN_2), memberName.c_str());
        else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
            snprintf(msg, sizeof(msg), strings.c_str(STRID_QUIT_CLAN_2), memberName.c_str());

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg);
        pPlayer->sendPacket(&gcSystemMessage);
    } else {
        // The case where the guild master is not on the same server. how?
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)


#endif

    __END_DEBUG_EX __END_CATCH
}
