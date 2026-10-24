//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyGuildMemberHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGModifyGuildMember.h"

#ifdef __GAME_SERVER__
#include "GCSystemMessage.h"
#include "GSExpelGuildMember.h"
#include "GSModifyGuildMember.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "PlayerCreature.h"
#include "SharedServerManager.h"
#include "StringPool.h"
#include "SystemAvailabilitiesManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGModifyGuildMemberHandler::execute(CGModifyGuildMember* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

    GuildManager& guilds = de::gameContext().guilds();

    StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__


    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SYSTEM_ASSERT(SYSTEM_GUILD);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPlayerCreature != NULL);

    // Get the guild.
    Guild* pGuild = guilds.getGuild(pPlayerCreature->getGuildID());
    // try { Assert(pGuild != NULL); } catch (Throwable& t ) { return; }
    if (pGuild == NULL)
        return;

    // Get the guild member information.
    GuildMember* pGuildMember = pGuild->getMember(pPlayerCreature->getName());
    // try { Assert(pGuild != NULL); } catch (Throwable& t ) { return; }
    if (pGuildMember == NULL)
        return;


    if (pPacket->getGuildMemberRank() == GuildMember::GUILDMEMBER_RANK_DENY) {
        ////////////////////////////////////////////////////////
        // Expel a guild member.
        ////////////////////////////////////////////////////////

        // Only the master can expel.
        if (pGuildMember->getRank() != GuildMember::GUILDMEMBER_RANK_MASTER)
            return;

        if (guilds.hasActiveWar(pGuild->getID())) {
            GCSystemMessage msg;
            msg.setMessage(strings.getString(STRID_CANNOT_KICK_DURING_WAR));
            pPlayer->sendPacket(&msg);

            return;
        }

        GSExpelGuildMember gsExpelGuildMember;
        gsExpelGuildMember.setGuildID(pGuild->getID());
        gsExpelGuildMember.setName(pPacket->getName());
        gsExpelGuildMember.setSender(pPlayerCreature->getName());

        de::gameContext().sharedServer().sendPacket(&gsExpelGuildMember);
    } else {
        if (pGuild->getActiveMemberCount() >= MAX_GUILDMEMBER_ACTIVE_COUNT) {
            GCSystemMessage msg;
            msg.setMessage(strings.getString(STRID_CANNOT_ACCEPT_MORE_JOIN));
            pPlayer->sendPacket(&msg);

            return;
        }
        ///////////////////////////////////////////////////////
        // Approve a guild join.
        ///////////////////////////////////////////////////////

        // Must be the master or a submaster.
        if (pGuildMember->getRank() != GuildMember::GUILDMEMBER_RANK_MASTER &&
            pGuildMember->getRank() != GuildMember::GUILDMEMBER_RANK_SUBMASTER)
            return;

        if (guilds.hasActiveWar(pGuild->getID())) {
            GCSystemMessage msg;
            msg.setMessage(strings.getString(STRID_CANNOT_ACCEPT_DURING_WAR));
            pPlayer->sendPacket(&msg);

            return;
        }

        GSModifyGuildMember gsModifyGuildMember;
        gsModifyGuildMember.setGuildID(pGuild->getID());
        gsModifyGuildMember.setName(pPacket->getName());
        gsModifyGuildMember.setGuildMemberRank(pPacket->getGuildMemberRank());
        gsModifyGuildMember.setSender(pPlayerCreature->getName());

        de::gameContext().sharedServer().sendPacket(&gsModifyGuildMember);
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
