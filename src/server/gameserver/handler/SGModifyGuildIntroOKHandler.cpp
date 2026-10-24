//----------------------------------------------------------------------
//
// Filename    : SGModifyGuildIntroOKHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameContext.h"
#include "Properties.h"
#include "SGModifyGuildIntroOK.h"

#ifdef __GAME_SERVER__

#include "DB.h"
#include "GCSystemMessage.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Player.h"

#endif

//----------------------------------------------------------------------
//
// SGModifyGuildIntroOKHandler::execute()
//
//----------------------------------------------------------------------
void SGModifyGuildIntroOKHandler::execute(SGModifyGuildIntroOK* pPacket)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__


        Assert(pPacket != NULL);

    // Get the guild.
    Guild* pGuild = de::gameContext().guilds().getGuild(pPacket->getGuildID());
    try {
        Assert(pGuild != NULL);
    } catch (Throwable&) {
        return;
    }

    pGuild->setIntro(pPacket->getGuildIntro());

#endif

    __END_DEBUG_EX
}
