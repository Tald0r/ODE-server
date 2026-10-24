//----------------------------------------------------------------------
//
// Filename    : GGGuildChatHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GGGuildChat.h"

#ifdef __GAME_SERVER__

#include "Creature.h"
#include "GCGuildChat.h"
#include "GameContext.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "PCFinder.h"
#include "Player.h"

static void broadcastGuild(Guild* pGuild, Packet* pPacket) {
    if (pGuild == NULL || pPacket == NULL)
        return;

    PCFinder& pcFinder = de::gameContext().playerCreatures();

    list<string> currentMembers = pGuild->getCurrentMembers();
    list<string>::const_iterator itr = currentMembers.begin();
    for (; itr != currentMembers.end(); itr++) {
        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pCreature = pcFinder.getCreature_LOCKED((*itr));
        if (pCreature != NULL) {
            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);

            if (pPlayer->getSocket() != NULL)
                pPlayer->sendPacket(pPacket);
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)
    }
}

#endif

//----------------------------------------------------------------------
//
// GGGuildChatHander::execute()
//
//----------------------------------------------------------------------
void GGGuildChatHandler::execute(GGGuildChat* pPacket)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

        GuildManager& guilds = de::gameContext().guilds();

#ifdef __GAME_SERVER__

    // Get the guild's currently connected members.
    Guild* pGuild = guilds.getGuild(pPacket->getGuildID());

    if (pGuild == NULL) {
        filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d", (int)pPacket->getGuildID());
        return;
    }

    // Build the guild chat packet.
    GCGuildChat gcGuildChat;
    gcGuildChat.setType(pPacket->getType());
    gcGuildChat.setSendGuildName(pGuild->getName());
    gcGuildChat.setSender(pPacket->getSender());
    gcGuildChat.setColor(pPacket->getColor());
    gcGuildChat.setMessage(pPacket->getMessage());

    if (pPacket->getType() == 0) {
        broadcastGuild(pGuild, &gcGuildChat);
    } else {
        // Union chat. A union dissolved since the lookup lists no members, so
        // it counts as no union and the chat stays in the guild.
        GuildUnion* pUnion = GuildUnionManager::Instance().getGuildUnion(pGuild->getID());
        if (pUnion == NULL || pUnion->isRetired()) {
            broadcastGuild(pGuild, &gcGuildChat);
        } else {
            list<GuildID_t> gList = pUnion->getGuildList();
            list<GuildID_t>::iterator itr = gList.begin();

            for (; itr != gList.end(); ++itr) {
                pGuild = guilds.getGuild(*itr);
                broadcastGuild(pGuild, &gcGuildChat);
            }

            pGuild = guilds.getGuild(pUnion->getMasterGuildID());
            broadcastGuild(pGuild, &gcGuildChat);
        }
    }

#endif

    __END_DEBUG
    __END_DEBUG_EX
}
