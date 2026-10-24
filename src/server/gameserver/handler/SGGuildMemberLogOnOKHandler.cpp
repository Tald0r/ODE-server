//----------------------------------------------------------------------
//
// Filename    : SGGuildMemberLogOnOKHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "GameContext.h"
#include "Properties.h"
#include "SGGuildMemberLogOnOK.h"

#ifdef __GAME_SERVER__

#include "DB.h"
#include "Guild.h"
#include "GuildManager.h"

#endif

//----------------------------------------------------------------------
//
// SGGuildMemberLogOnOKHandler::execute()
//
//----------------------------------------------------------------------
void SGGuildMemberLogOnOKHandler::execute(SGGuildMemberLogOnOK* pPacket)

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

    // Check whether it is a guild member.
    GuildMember* pGuildMember = pGuild->getMember(pPacket->getName());
    try {
        Assert(pGuildMember != NULL);
    } catch (Throwable&) {
        return;
    }

    pGuildMember->setLogOn(pPacket->getLogOn());
    pGuildMember->setServerID(pPacket->getServerID());

#endif

    __END_DEBUG_EX
}
