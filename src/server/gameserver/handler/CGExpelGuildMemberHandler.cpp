//////////////////////////////////////////////////////////////////////////////
// Filename    : CGExpelGuildMemberHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGExpelGuildMember.h"
#include "GamePlayer.h"

#ifdef __GAME_SERVER__
#include "GSExpelGuildMember.h"
#include "GameContext.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "SharedServerManager.h"
#include "Slayer.h"
#include "SystemAvailabilitiesManager.h"
#include "Vampire.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGExpelGuildMemberHandler::execute(CGExpelGuildMember* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPlayerCreature != NULL);

    SYSTEM_ASSERT(SYSTEM_GUILD);

    // Get the guild.
    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());
    try {
        Assert(pGuild != NULL);
    } catch (Throwable& t) {
        return;
    }

    // Check whether the player is the guild's master.
    if (pGuild->getMaster() != pPlayerCreature->getName()) {
        // Not the master. Ignore
        return;
    }

    GSExpelGuildMember gsExpelGuildMember;
    gsExpelGuildMember.setGuildID(pGuild->getID());
    gsExpelGuildMember.setName(pPlayerCreature->getName());
    gsExpelGuildMember.setSender(pPlayerCreature->getName());

    de::gameContext().sharedServer().sendPacket(&gsExpelGuildMember);

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
