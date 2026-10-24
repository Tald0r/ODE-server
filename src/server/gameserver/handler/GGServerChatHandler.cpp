//----------------------------------------------------------------------
//
// Filename    : GGServerChatHandler.cpp
// Written By  : inthesky
// Description : Carries out Whisper Chat between servers.
//
//----------------------------------------------------------------------

// include files
#include "GGServerChat.h"

#ifdef __GAME_SERVER__

#include "Creature.h"
#include "GCWhisper.h"
#include "GCWhisperFailed.h"
#include "GameContext.h"
#include "Guild.h"
#include "GuildManager.h"
#include "PCFinder.h"
#include "Player.h"

#endif

//----------------------------------------------------------------------
//
// GGServerChatHander::execute()
//
//----------------------------------------------------------------------
void GGServerChatHandler::execute(GGServerChat* pPacket)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

        PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    Creature* pCreature = pcFinder.getCreature_LOCKED(pPacket->getReceiver());
    if (pCreature != NULL && pCreature->getPlayer() != NULL) {
        GCWhisper gcWhisper;

        gcWhisper.setName(pPacket->getSender());
        gcWhisper.setColor(pPacket->getColor());
        gcWhisper.setMessage(pPacket->getMessage());
        gcWhisper.setRace(pPacket->getRace());

        pCreature->getPlayer()->sendPacket(&gcWhisper);
    }
    __LEAVE_CRITICAL_SECTION(pcFinder)


#endif

    __END_DEBUG
    __END_DEBUG_EX
}
