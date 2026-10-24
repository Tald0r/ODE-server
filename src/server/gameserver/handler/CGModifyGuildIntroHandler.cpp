//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyGuildIntroHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGModifyGuildIntro.h"

#ifdef __GAME_SERVER__
#include "GSModifyGuildIntro.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "PlayerCreature.h"
#include "SharedServerManager.h"
#include "SystemAvailabilitiesManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGModifyGuildIntroHandler::execute(CGModifyGuildIntro* pPacket, Player* pPlayer)

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

    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPlayerCreature != NULL);

    // Get the guild.
    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());
    if (pGuild == NULL)
        return;

    // Get the guild member information.
    GuildMember* pGuildMember = pGuild->getMember(pPlayerCreature->getName());
    if (pGuildMember == NULL)
        return;

    // Must be the guild master.
    if (pGuild->getMaster() != pPlayerCreature->getName())
        return;
    if (pGuildMember->getRank() != GuildMember::GUILDMEMBER_RANK_MASTER)
        return;

    // Send the guild Intro change packet to the shared server.
    GSModifyGuildIntro gsModifyGuildIntro;
    gsModifyGuildIntro.setGuildID(pGuild->getID());
    gsModifyGuildIntro.setGuildIntro(pPacket->getGuildIntro());

    de::gameContext().sharedServer().sendPacket(&gsModifyGuildIntro);

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
