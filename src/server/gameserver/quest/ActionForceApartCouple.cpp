////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionForceApartCouple.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionForceApartCouple.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "SystemAvailabilitiesManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionForceApartCouple::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionForceApartCouple::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_COUPLE);

    GCNPCResponse gcNPCResponse;
    gcNPCResponse.setCode(NPC_RESPONSE_APART_COUPLE_FORCE);

    Player* pPlayer = pCreature2->getPlayer();
    pPlayer->sendPacket(&gcNPCResponse);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionForceApartCouple::toString() const

{
    StringStream msg;
    msg << "ActionForceApartCouple(" << ")";

    return msg.toString();
}
