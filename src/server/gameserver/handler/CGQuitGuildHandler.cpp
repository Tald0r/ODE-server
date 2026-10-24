//////////////////////////////////////////////////////////////////////////////
// Filename    : CGQuitGuildHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGQuitGuild.h"
#include "GamePlayer.h"

#ifdef __GAME_SERVER__
#include "GSQuitGuild.h"
#include "GameContext.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "SharedServerManager.h"
#include "SystemAvailabilitiesManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGQuitGuildHandler::execute(CGQuitGuild* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SYSTEM_ASSERT(SYSTEM_GUILD);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPlayerCreature != NULL);

    // Get the guild the player belongs to.
    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());
    try {
        Assert(pGuild != NULL);
    } catch (Throwable& t) {
        return;
    }

    // Check that the player is a member of the guild. A member the guild let
    // go is retired rather than freed, so the pointer stays readable and says
    // so; quitting a guild one has already left is nothing to forward.
    GuildMember* pGuildMember = pGuild->getMember(pPlayerCreature->getName());
    if (pGuildMember == NULL || pGuildMember->isRetired())
        return;

    GSQuitGuild gsQuitGuild;
    gsQuitGuild.setGuildID(pGuild->getID());
    gsQuitGuild.setName(pPlayerCreature->getName());

    de::gameContext().sharedServer().sendPacket(&gsQuitGuild);

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
