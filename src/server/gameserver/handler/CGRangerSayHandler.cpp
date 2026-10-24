//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRangerSayHandler.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRangerSay.h"

#ifdef __GAME_SERVER__
#include "BroadcastFilter.h"
#include "Creature.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "StringStream.h"
#include "ZoneGroupManager.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRangerSayHandler::execute(CGRangerSay* pPacket, Player* pPlayer)

{
#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    // Put the creature name and the message into the packet.
    StringStream msg;
    msg << pCreature->getName() << " " << pPacket->getMessage();

    Race_t race = pCreature->getRace();

    // Build the packet
    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(msg.toString());
    gcSystemMessage.setType(SYSTEM_MESSAGE_RANGER_SAY);

    // Build the filter
    BroadcastFilterRace filter(race);

    // Broadcast to every user
    de::gameContext().zoneGroups().pushBroadcastPacket(&gcSystemMessage, &filter);

#endif
}
