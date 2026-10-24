//----------------------------------------------------------------------
//
// Filename    : SGModifyGuildOKHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameContext.h"
#include "Properties.h"
#include "SGModifyGuildOK.h"

#ifdef __GAME_SERVER__

#include "DB.h"
#include "GCModifyGuildMemberInfo.h"
#include "GCOtherModifyInfo.h"
#include "GCSystemMessage.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "PlayerMailbox.h"
#include "Properties.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "repository/MessageRepository.h"

#endif

//----------------------------------------------------------------------
//
// SGModifyGuildOKHandler::execute()
//
//----------------------------------------------------------------------
void SGModifyGuildOKHandler::execute(SGModifyGuildOK* pPacket)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());
    Assert(pGuild != NULL);

    if (pGuild->getState() == Guild::GUILD_STATE_WAIT && pPacket->getGuildState() == Guild::GUILD_STATE_ACTIVE) {
        /////////////////////////////////////////////////////////////
        // Add the zone
        /////////////////////////////////////////////////////////////

        // Change it into a regular guild
        pGuild->setState(Guild::GUILD_STATE_ACTIVE);

        HashMapGuildMember& Members = pGuild->getMembers_NOLOCKED();
        HashMapGuildMemberItor itr = Members.begin();
        for (; itr != Members.end(); itr++) {
            GuildMember* pGuildMember = itr->second;

            // If the member is online, apply the guild id on the owning zone
            // thread (PlayerMailbox.h), with the guild facts captured by
            // value: the member map is walked here and now, the command
            // runs a tick later.
            const string memberName = pGuildMember->getName();
            const GuildID_t guildID = pGuild->getID();
            const string guildName = pGuild->getName();
            const GuildMemberRank_t rank = pGuildMember->getRank();
            de::postToPlayer(memberName, [=](PlayerCreature& pc, Player& player) {
                // Change the guild id.
                pc.setGuildID(guildID);

                // Tell the client that the guild id changed.
                GCModifyGuildMemberInfo gcModifyGuildMemberInfo;
                gcModifyGuildMemberInfo.setGuildID(guildID);
                gcModifyGuildMemberInfo.setGuildName(guildName);
                gcModifyGuildMemberInfo.setGuildMemberRank(rank);
                player.sendPacket(&gcModifyGuildMemberInfo);

                // Tell those around.
                Zone* pZone = pc.getZone();
                Assert(pZone != NULL);

                GCOtherModifyInfo gcOtherModifyInfo;
                gcOtherModifyInfo.setObjectID(pc.getObjectID());
                gcOtherModifyInfo.addShortData(MODIFY_GUILDID, pc.getGuildID());

                pZone->broadcastPacket(pc.getX(), pc.getY(), &gcOtherModifyInfo, &pc);

                // Report that it became a regular guild
                MessageRepository& messages = defaultMessageRepository();
                vector<string> queued = messages.loadMessages(memberName);

                for (size_t m = 0; m < queued.size(); m++) {
                    GCSystemMessage gcSystemMessage;
                    gcSystemMessage.setMessage(queued[m]);
                    player.sendPacket(&gcSystemMessage);
                }

                messages.deleteMessages(memberName);
            });
        }
    }

#endif

    __END_DEBUG_EX
}
