//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyGuildMemberIntroHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGModifyGuildMemberIntro.h"

#ifdef __GAME_SERVER__
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "PlayerCreature.h"
#include "SystemAvailabilitiesManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGModifyGuildMemberIntroHandler::execute(CGModifyGuildMemberIntro* pPacket, Player* pPlayer)

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
    Guild* pGuild = de::gameContext().guilds().getGuild(pPlayerCreature->getGuildID());
    if (pGuild == NULL)
        return;

    // Get the guild member information. A member the guild let go is retired
    // rather than freed, so the pointer stays readable and says so; there is
    // no introduction to write for someone who is no longer a member.
    GuildMember* pGuildMember = pGuild->getMember(pPlayerCreature->getName());
    if (pGuildMember == NULL || pGuildMember->isRetired())
        return;

    pGuildMember->saveIntro(pPacket->getGuildMemberIntro());

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
