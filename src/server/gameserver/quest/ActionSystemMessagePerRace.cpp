////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSystemMessagePerRace.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSystemMessagePerRace.h"

#include "Creature.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "StringPool.h"

////////////////////////////////////////////////////////////////////////////////
// read from property buffer
////////////////////////////////////////////////////////////////////////////////
void ActionSystemMessagePerRace::read(PropertyBuffer& propertyBuffer)

{}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionSystemMessagePerRace::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCSystemMessage gcSystemMessage;

    if (pCreature2->isSlayer())
        gcSystemMessage.setMessage(context().strings().getString(STRID_CANNOT_ENTER_SUM_NEWBIE_ZONE));
    else
        gcSystemMessage.setMessage(context().strings().getString(STRID_CANNOT_ENTER_LEVEL_NEWBIE_ZONE));

    pPlayer->sendPacket(&gcSystemMessage);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSystemMessagePerRace::toString() const

{
    StringStream msg;
    msg << "ActionSystemMessagePerRace(" << ")";
    return msg.toString();
}
