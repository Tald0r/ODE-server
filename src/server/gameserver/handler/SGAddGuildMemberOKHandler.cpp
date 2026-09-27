//----------------------------------------------------------------------
//
// Filename    : SGAddGuildMemberOKHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameContext.h"
#include "Properties.h"
#include "SGAddGuildMemberOK.h"

#ifdef __GAME_SERVER__

#include <stdio.h>

#include "DB.h"
#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCSystemMessage.h"
#include "Guild.h"
#include "GuildManager.h"
#include "KernelContext.h"
#include "PCFinder.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "PlayerMailbox.h"
#include "Properties.h"
#include "StringPool.h"
#include "StringStream.h"
#include "Zone.h"
#include "repository/GoldRepository.h"

#endif

//----------------------------------------------------------------------
//
// SGAddGuildMemberOKHandler::execute()
//
//----------------------------------------------------------------------
void SGAddGuildMemberOKHandler::execute(SGAddGuildMemberOK* pPacket)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

        StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__

    // Build the guild member object.
    GuildMember* pGuildMember = new GuildMember();
    pGuildMember->setGuildID(pPacket->getGuildID());
    pGuildMember->setName(pPacket->getName());
    pGuildMember->setRank(pPacket->getGuildMemberRank());

    // Add it to the guild.
    Guild* pGuild = de::gameContext().guilds().getGuild(pGuildMember->getGuildID());
    pGuild->addMember(pGuildMember);

    // Send the member a message.
    //
    // A master/submaster pays the fee: from the in-memory gold if the
    // member is online, from the database row if not. The in-memory path
    // mutates the creature, so it runs on the owning zone thread
    // (PlayerMailbox.h) with the guild facts captured by value; the
    // database path doubles as the ifGone fallback, so a member who logs
    // out between the post and the tick is still charged.
    const string memberName = pGuildMember->getName();
    const GuildMemberRank_t rank = pGuildMember->getRank();
    const GuildRace_t guildRace = pGuild->getRace();

    Gold_t Fee;
    if (rank == GuildMember::GUILDMEMBER_RANK_MASTER)
        Fee = REQUIRE_SLAYER_MASTER_GOLD;
    else if (rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER)
        Fee = REQUIRE_SLAYER_SUBMASTER_GOLD;
    else
        Fee = 0;

    // Not connected: for a master or a submaster, take the money from the DB.
    const bool addedHere = pPacket->getServerGroupID() == de::kernelContext().config().getPropertyInt("ServerID");
    de::GoneCommand chargeInDatabase = [=] {
        if ((rank == GuildMember::GUILDMEMBER_RANK_MASTER ||
             rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) // when it is the guild master or a submaster
            && addedHere)                                     // was this guild member added on this game server?
        {
            // The race decides which table the row is in. A guild whose
            // race is none of the three named none, and the write was
            // skipped; hasRaceTable keeps that guard, since CharacterRace
            // has no "no table" value.
            CharacterRace race = CHARACTER_RACE_SLAYER;
            bool hasRaceTable = false;
            if (guildRace == Guild::GUILD_RACE_SLAYER) {
                race = CHARACTER_RACE_SLAYER;
                hasRaceTable = true;
            } else if (guildRace == Guild::GUILD_RACE_VAMPIRE) {
                race = CHARACTER_RACE_VAMPIRE;
                hasRaceTable = true;
            } else if (guildRace == Guild::GUILD_RACE_OUSTERS) {
                race = CHARACTER_RACE_OUSTERS;
                hasRaceTable = true;
            }

            if (hasRaceTable && Fee != 0) {
                defaultGoldRepository().decreaseGoldClamped(memberName, race, Fee);
            }
        }
    };

    const bool online = de::postToPlayer(
        memberName,
        [=](PlayerCreature& pc, Player& player) {
            if (rank == GuildMember::GUILDMEMBER_RANK_MASTER ||
                rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) // when it is the guild master or a submaster
            {
                Gold_t CurMoney = pc.getGold();
                if (CurMoney < Fee) {
                    // This is bad
                    CurMoney = 0;
                } else
                    CurMoney -= Fee;

                pc.setGoldEx(CurMoney);

                if (Fee != 0) {
                    GCModifyInformation gcModifyInformation;
                    gcModifyInformation.addLongData(MODIFY_GOLD, CurMoney);

                    // Send the changed information to the client.
                    player.sendPacket(&gcModifyInformation);
                }

                // Show the guild join message.
                GCSystemMessage gcSystemMessage;
                if (guildRace == Guild::GUILD_RACE_SLAYER)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_TEAM_JOIN_ACCEPTED));
                else if (guildRace == Guild::GUILD_RACE_VAMPIRE)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_CLAN_JOIN_ACCEPTED));
                else if (guildRace == Guild::GUILD_RACE_OUSTERS)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_CLAN_JOIN_ACCEPTED));
                player.sendPacket(&gcSystemMessage);

            } else if (rank == GuildMember::GUILDMEMBER_RANK_WAIT) {
                // Send the guild join request message.
                GCSystemMessage gcSystemMessage;
                if (guildRace == Guild::GUILD_RACE_SLAYER)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_TEAM_JOIN_TRY));
                else if (guildRace == Guild::GUILD_RACE_VAMPIRE)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_CLAN_JOIN_TRY));
                else if (guildRace == Guild::GUILD_RACE_OUSTERS)
                    gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_CLAN_JOIN_TRY));

                player.sendPacket(&gcSystemMessage);
            }
        },
        chargeInDatabase);

    if (!online)
        chargeInDatabase();

    // Send the guild master a message. (send only: fine from this thread)
    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    Creature* pCreature = pcFinder.getCreature_LOCKED(pGuild->getMaster());
    if (pCreature != NULL && pCreature->isPC() && pGuildMember->getRank() != GuildMember::GUILDMEMBER_RANK_MASTER) {
        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer != NULL);

        char msg[100];

        if (pGuildMember->getRank() == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_TEAM_JOIN_ACCEPTED_2), pGuildMember->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_JOIN_ACCEPTED_2), pGuildMember->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_JOIN_ACCEPTED_2), pGuildMember->getName().c_str());
        } else if (pGuildMember->getRank() == GuildMember::GUILDMEMBER_RANK_WAIT) {
            if (pGuild->getRace() == Guild::GUILD_RACE_SLAYER)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_TEAM_JOIN_TRY_2), pGuildMember->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_VAMPIRE)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_JOIN_TRY_2), pGuildMember->getName().c_str());
            else if (pGuild->getRace() == Guild::GUILD_RACE_OUSTERS)
                snprintf(msg, sizeof(msg), strings.c_str(STRID_CLAN_JOIN_TRY_2), pGuildMember->getName().c_str());
        }

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg);
        pPlayer->sendPacket(&gcSystemMessage);
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)

#endif

    __END_DEBUG_EX __END_CATCH
}
