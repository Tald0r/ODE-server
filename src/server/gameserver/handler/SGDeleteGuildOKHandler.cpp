//----------------------------------------------------------------------
//
// Filename    : SGDeleteGuildOKHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameContext.h"
#include "SGDeleteGuildOK.h"

#ifdef __GAME_SERVER__

#include <string>
#include <utility>
#include <vector>

#include "Assert1.h"
#include "DB.h"
#include "GCModifyGuildMemberInfo.h"
#include "GCModifyInformation.h"
#include "GCOtherModifyInfo.h"
#include "GCSystemMessage.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "PlayerMailbox.h"
#include "Properties.h"
#include "ResurrectLocationManager.h"
#include "Zone.h"
#include "repository/MessageRepository.h"
#endif

//----------------------------------------------------------------------
//
// SGDeleteGuildOKHandler::execute()
//
//----------------------------------------------------------------------
void SGDeleteGuildOKHandler::execute(SGDeleteGuildOK* pPacket)

{
    GuildManager& guilds = de::gameContext().guilds();

#ifdef __GAME_SERVER__

    // Warp the members inside the guild hideout.
    // Delete the guild hideout.
    // Warping the members and deleting the hideout would be safest done inside the ZoneGroup Thread from another thread.
    // For now it is left alone. The Portal is blocked, so it cannot be entered again.

    Assert(pPacket != NULL);

    // Get the guild.
    Guild* pGuild = guilds.getGuild(pPacket->getGuildID());
    try {
        Assert(pGuild != NULL);
    } catch (Throwable&) {
        return;
    }


    // Tell whether the guild is disbanded while active or while waiting.
    if (pGuild->getState() == Guild::GUILD_STATE_ACTIVE) {
        // Take the members out under the guild mutex -- a zone thread may
        // be copying the member list at this moment (CGSelectGuild) -- and
        // work from the returned names. The GuildMember objects and the
        // Guild itself are retired, never freed here: readers hold raw
        // pointers to both across the lock (see Guild::m_RetiredMembers,
        // GuildManager::m_RetiredGuilds).
        const std::vector<std::pair<std::string, GuildMemberRank_t>> members = pGuild->retireAllMembers();

        for (size_t i = 0; i < members.size(); i++) {
            const std::string& memberName = members[i].first;

            // If the member is online, reset its guild id on the owning zone
            // thread (PlayerMailbox.h). Nothing from this handler is
            // captured by pointer: the member and the guild are retired
            // below, before the command runs.
            // That also means the guild is unregistered one tick before the
            // member's guild id is reset; the
            // readers of a creature's guild id all null-check the lookup
            // (GuildMissing.log), so the window shows as a stale badge, not
            // a crash.
            de::postToPlayer(memberName, [](PlayerCreature& pc, Player& player) {
                // Change the Slayer's and the Vampire's guild id.
                if (pc.isSlayer()) {
                    pc.setGuildID(99); // the guild ID of a Slayer that has not joined

                    // Tell the client that the guild id changed.
                    GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
                    gcModifyGuildMemberInfo.setGuildID(pc.getGuildID());
                    gcModifyGuildMemberInfo.setGuildName("");
                    gcModifyGuildMemberInfo.setGuildMemberRank(GuildMember::GUILDMEMBER_RANK_DENY);
                    player.sendPacket(&gcModifyGuildMemberInfo);
                } else if (pc.isVampire()) {
                    pc.setGuildID(0); // the guild ID of a Vampire that has not joined

                    // Tell the client that the guild id changed.
                    GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
                    gcModifyGuildMemberInfo.setGuildID(pc.getGuildID());
                    gcModifyGuildMemberInfo.setGuildName("");
                    gcModifyGuildMemberInfo.setGuildMemberRank(GuildMember::GUILDMEMBER_RANK_DENY);
                    player.sendPacket(&gcModifyGuildMemberInfo);
                } else if (pc.isOusters()) {
                    pc.setGuildID(66); // the guild ID of an Ousters that has not joined

                    // Tell the client that the guild id changed.
                    GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
                    gcModifyGuildMemberInfo.setGuildID(pc.getGuildID());
                    gcModifyGuildMemberInfo.setGuildName("");
                    gcModifyGuildMemberInfo.setGuildMemberRank(GuildMember::GUILDMEMBER_RANK_DENY);
                    player.sendPacket(&gcModifyGuildMemberInfo);
                }

                // Tell the clients around that the guild id changed.
                GCOtherModifyInfo gcOtherModifyInfo;
                gcOtherModifyInfo.setObjectID(pc.getObjectID());
                gcOtherModifyInfo.addShortData(MODIFY_GUILDID, pc.getGuildID());

                Zone* pZone = pc.getZone();
                Assert(pZone != NULL);

                pZone->broadcastPacket(pc.getX(), pc.getY(), &gcOtherModifyInfo, &pc);
            });
        }

        // Settle the guild's union standing first: nothing but this call
        // keeps the union tables in step with the guilds, and it reads the
        // guild masters it notifies out of the GuildManager, so it has to run
        // while the guild is still in there.
        GuildUnionManager::Instance().removeGuildFromUnion(pGuild->getID());

        // Delete the guild from the guild manager (retired, not freed).
        guilds.deleteGuild(pGuild->getID());
    } else if (pGuild->getState() == Guild::GUILD_STATE_WAIT) {
        const std::vector<std::pair<std::string, GuildMemberRank_t>> members = pGuild->retireAllMembers();

        for (size_t i = 0; i < members.size(); i++) {
            const std::string& memberName = members[i].first;

            // If the member is online, refund the fee on the owning zone
            // thread (PlayerMailbox.h); the rank is captured by value
            // taken from the returned list, not a live member. The
            // message repository is looked up inside the command so the
            // SQL runs on the zone thread's own connection. A SQL failure
            // there surfaces as the DatabaseError END_DB throws -- not a
            // Throwable -- which the mailbox drain logs without stopping
            // the tick.
            const GuildMemberRank_t rank = members[i].second;
            de::postToPlayer(memberName, [rank](PlayerCreature& pc, Player& player) {
                // Refund the registration fee.
                Gold_t Gold = pc.getGold();
                if (rank == GuildMember::GUILDMEMBER_RANK_MASTER) {
                    Gold = min((uint64_t)(Gold + RETURN_SLAYER_MASTER_GOLD), (uint64_t)2000000000);
                } else if (rank == GuildMember::GUILDMEMBER_RANK_SUBMASTER) {
                    Gold = min((uint64_t)(Gold + RETURN_SLAYER_SUBMASTER_GOLD), (uint64_t)2000000000);
                }

                pc.setGoldEx(Gold);

                GCModifyInformation gcModifyInformation;
                gcModifyInformation.addLongData(MODIFY_GOLD, Gold);
                player.sendPacket(&gcModifyInformation);

                // Send the message.
                MessageRepository& messages = defaultMessageRepository();
                vector<string> queued = messages.loadMessages(pc.getName());

                for (size_t m = 0; m < queued.size(); m++) {
                    GCSystemMessage message;
                    message.setMessage(queued[m]);
                    player.sendPacket(&message);
                }

                messages.deleteMessages(pc.getName());
            });
        }

        // Settle the guild's union standing while the guild is still in the
        // GuildManager, as above.
        GuildUnionManager::Instance().removeGuildFromUnion(pGuild->getID());

        // Delete the guild from the guild manager (retired, not freed).
        guilds.deleteGuild(pGuild->getID());
    }

#endif
}
