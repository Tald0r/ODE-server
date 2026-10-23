////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionWarpToResurrectPosition.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionWarpToResurrectPosition.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "ResurrectLocationManager.h"
#include "ZoneUtil.h"
#include "couple/PartnerWaitingManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionWarpToResurrectPosition::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionWarpToResurrectPosition::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    if (pPC == NULL)
        return;

    ZONE_COORD zoneCoord;
    context().resurrectLocations().getPosition(pPC, zoneCoord);

    transportCreature(pCreature2, zoneCoord.id, zoneCoord.x, zoneCoord.y);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionWarpToResurrectPosition::toString() const

{
    StringStream msg;
    msg << "ActionWarpToResurrectPosition(" << ")";

    return msg.toString();
}
