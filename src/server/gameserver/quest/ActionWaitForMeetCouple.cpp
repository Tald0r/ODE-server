////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionWaitForMeetCouple.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionWaitForMeetCouple.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "SystemAvailabilitiesManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionWaitForMeetCouple::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionWaitForMeetCouple::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_COUPLE);

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);


    GCNPCResponse gcNPCResponse;
    gcNPCResponse.setCode(NPC_RESPONSE_WAIT_FOR_MEET_COUPLE);

    Player* pPlayer = pCreature2->getPlayer();
    pPlayer->sendPacket(&gcNPCResponse);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionWaitForMeetCouple::toString() const

{
    StringStream msg;
    msg << "ActionWaitForMeetCouple(" << ")";

    return msg.toString();
}
