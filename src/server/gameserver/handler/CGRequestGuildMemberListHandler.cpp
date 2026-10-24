//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestGuildMemberListHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRequestGuildMemberList.h"

#ifdef __GAME_SERVER__
#include "GCGuildMemberList.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildMemberInfo.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "SystemAvailabilitiesManager.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRequestGuildMemberListHandler::execute(CGRequestGuildMemberList* pPacket, Player* pPlayer)

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
    // try { Assert(pGuild != NULL); } catch (Throwable& ) { return; }
    if (pGuild == NULL)
        return;

    GCGuildMemberList gcGuildMemberList;

    pGuild->makeMemberInfo(gcGuildMemberList);

    pPlayer->sendPacket(&gcGuildMemberList);

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
