//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSelectGuildMemberHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSelectGuildMember.h"

#ifdef __GAME_SERVER__
#include "GCShowGuildMemberInfo.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "PlayerCreature.h"
#include "SystemAvailabilitiesManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSelectGuildMemberHandler::execute(CGSelectGuildMember* pPacket, Player* pPlayer)

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

    // Get the selected guild.
    Guild* pGuild = de::gameContext().guilds().getGuild(pPlayerCreature->getGuildID());
    // try { Assert( pGuild != NULL ); } catch ( Throwable& ) { return; }
    if (pGuild == NULL)
        return;

    // Get the selected guild member. A member the guild let go is retired
    // rather than freed, so the pointer stays readable and says so; its rank
    // and introduction are the ones it left with, not a member's to show.
    GuildMember* pGuildMember = pGuild->getMember(pPacket->getName());
    // try { Assert( pGuildMember != NULL ); } catch ( Throwable& ) { return; }
    if (pGuildMember == NULL || pGuildMember->isRetired())
        return;

    GCShowGuildMemberInfo gcShowGuildMemberInfo;
    gcShowGuildMemberInfo.setGuildID(pGuildMember->getGuildID());
    gcShowGuildMemberInfo.setName(pGuildMember->getName());
    gcShowGuildMemberInfo.setGuildMemberRank(pGuildMember->getRank());
    gcShowGuildMemberInfo.setGuildMemberIntro(pGuildMember->getIntro());

    pPlayer->sendPacket(&gcShowGuildMemberInfo);

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
