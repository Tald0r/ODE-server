//----------------------------------------------------------------------
//
// Filename    : SGModifyGuildMemberOKHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameContext.h"
#include "Properties.h"
#include "SGModifyGuildMemberOK.h"

#ifdef __GAME_SERVER__

#include <stdio.h>

#include "DB.h"
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
#include "repository/MessageRepository.h"

#endif

//----------------------------------------------------------------------
//
// SGModifyGuildMemberOKHandler::execute()
//
//----------------------------------------------------------------------
void SGModifyGuildMemberOKHandler::execute(SGModifyGuildMemberOK* pPacket)

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

    if (pGuildMember->getRank() == GuildMember::GUILDMEMBER_RANK_WAIT &&
        pPacket->getGuildMemberRank() == GuildMember::GUILDMEMBER_RANK_NORMAL) {
        ////////////////////////////////////////////////////////////////////////////
        // This is the case where a guild join request was approved.
        ////////////////////////////////////////////////////////////////////////////

        // Change the guild member information.
        pGuild->modifyMemberRank(pGuildMember->getName(), pPacket->getGuildMemberRank());

        // If the new member is online, apply the guild id and tell the
        // zone. That mutates creature and zone state, so it runs on the
        // owning zone thread (PlayerMailbox.h); everything it needs is
        // captured by value because pGuildMember may be gone by then.
        {
            const string memberName = pGuildMember->getName();
            const GuildID_t memberGuildID = pGuildMember->getGuildID();
            const GuildID_t guildID = pGuild->getID();
            const string guildName = pGuild->getName();
            const GuildMemberRank_t rank = pGuildMember->getRank();
            de::postToPlayer(memberName, [=](PlayerCreature& pc, Player& player) {
                // Register the real guild ID.
                pc.setGuildID(memberGuildID);

                Zone* pZone = pc.getZone();
                Assert(pZone != NULL);

                // Send the changed guild ID information.
                GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
                gcModifyGuildMemberInfo.setGuildID(guildID);
                gcModifyGuildMemberInfo.setGuildName(guildName);
                gcModifyGuildMemberInfo.setGuildMemberRank(rank);
                player.sendPacket(&gcModifyGuildMemberInfo);

                // Send the guild join approval message.
                MessageRepository& messages = defaultMessageRepository();
                vector<string> queued = messages.loadMessages(memberName);

                for (size_t m = 0; m < queued.size(); m++) {
                    GCSystemMessage gcSystemMessage;
                    gcSystemMessage.setMessage(queued[m]);
                    player.sendPacket(&gcSystemMessage);
                }

                messages.deleteMessages(memberName);

                // Tell those around about the guild join.
                GCOtherModifyInfo gcOtherModifyInfo;
                gcOtherModifyInfo.setObjectID(pc.getObjectID());
                gcOtherModifyInfo.addShortData(MODIFY_GUILDID, memberGuildID);

                pZone->broadcastPacket(pc.getX(), pc.getY(), &gcOtherModifyInfo);
            });
        }

        // Send the one who approved a message. (send only: fine from this thread)
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pCreature = pcFinder.getCreature_LOCKED(pPacket->getSender());
        if (pCreature != NULL && pCreature->isPC()) {
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);


            char msg[100];
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_ACCEPT_TEAM_JOIN), pGuildMember->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_ACCEPT_CLAN_JOIN), pGuildMember->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_ACCEPT_CLAN_JOIN), pGuildMember->getName().c_str());

            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(msg);
            pPlayer->sendPacket(&gcSystemMessage);
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    } else if (pGuildMember->getRank() != GuildMember::GUILDMEMBER_RANK_MASTER &&
               pPacket->getGuildMemberRank() == GuildMember::GUILDMEMBER_RANK_MASTER) {
        ///////////////////////////////////////////////////////////
        // Change the guild master.
        ///////////////////////////////////////////////////////////

        string sMaster = pGuild->getMaster();

        // Give the guild master the original rank of the member that becomes the new master.
        pGuild->modifyMemberRank(sMaster, pGuildMember->getRank());
        // Set the new guild master's rank.
        pGuild->modifyMemberRank(pGuildMember->getName(), pPacket->getGuildMemberRank());
        // Set the new guild master on the guild object.
        pGuild->setMaster(pGuildMember->getName());

        // Send a message if connected.
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        // If the new guild master is on this game server, send it the new information.
        Creature* pCreature = pcFinder.getCreature_LOCKED(pGuildMember->getName());
        if (pCreature != NULL && pCreature->isPC()) {
            PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);
            Assert(pPlayerCreature != NULL);

            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);

            // Send the changed guild ID information.
            GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
            gcModifyGuildMemberInfo.setGuildID(pGuild->getID());
            gcModifyGuildMemberInfo.setGuildName(pGuild->getName());
            gcModifyGuildMemberInfo.setGuildMemberRank(pGuildMember->getRank());
            pPlayer->sendPacket(&gcModifyGuildMemberInfo);
        }

        // If the original guild master is on this game server, send it the new information.
        pCreature = pcFinder.getCreature_LOCKED(sMaster);
        if (pCreature != NULL && pCreature->isPC()) {
            PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);
            Assert(pPlayerCreature != NULL);

            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);

            // Send the changed guild ID information.
            GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
            gcModifyGuildMemberInfo.setGuildID(pGuild->getID());
            gcModifyGuildMemberInfo.setGuildName(pGuild->getName());
            gcModifyGuildMemberInfo.setGuildMemberRank(pGuildMember->getRank());
            pPlayer->sendPacket(&gcModifyGuildMemberInfo);
        }

        // Send the one who changed the master a message.
        pCreature = pcFinder.getCreature_LOCKED(pPacket->getSender());
        if (pCreature != NULL && pCreature->isPC()) {
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);


            char msg[200];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_MODIFY_GUILD_MASTER), pGuild->getName().c_str(),
                     sMaster.c_str(), pGuildMember->getName().c_str());

            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(msg);
            pPlayer->sendPacket(&gcSystemMessage);
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    } else {
        ///////////////////////////////////////////////////////////
        // Change the guild member information.
        ///////////////////////////////////////////////////////////
        pGuild->modifyMemberRank(pGuildMember->getName(), pPacket->getGuildMemberRank());

        // Send a message if connected.
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pCreature = pcFinder.getCreature_LOCKED(pGuildMember->getName());
        if (pCreature != NULL && pCreature->isPC()) {
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);

            GCSystemMessage gcSystemMessage;
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
                gcSystemMessage.setMessage(strings.getString(STRID_TEAM_RIGHT_CHANGED));
            else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
                gcSystemMessage.setMessage(strings.getString(STRID_CLAN_RIGHT_CHANGED));
            else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
                gcSystemMessage.setMessage(strings.getString(STRID_CLAN_RIGHT_CHANGED));

            pPlayer->sendPacket(&gcSystemMessage);
        }

        // Send the one who made the change a message.
        pCreature = pcFinder.getCreature_LOCKED(pPacket->getSender());
        if (pCreature != NULL && pCreature->isPC()) {
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);


            char msg[100];
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_TEAM_RIGHT_CHANGED_2), pGuildMember->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_RIGHT_CHANGED_2), pGuildMember->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_RIGHT_CHANGED_2), pGuildMember->getName().c_str());


            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(msg);
            pPlayer->sendPacket(&gcSystemMessage);
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
