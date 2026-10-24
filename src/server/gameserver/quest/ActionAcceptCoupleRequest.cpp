////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionAcceptCoupleRequest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionAcceptCoupleRequest.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "SystemAvailabilitiesManager.h"
#include "couple/PartnerWaitingManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionAcceptCoupleRequest::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionAcceptCoupleRequest::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_COUPLE);

    GCNPCResponse gcNPCResponse;

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

    Assert(pNPC != NULL);
    Assert(pPC != NULL);

    PartnerWaitingManager* pPWM = pNPC->getCoupleRegisterManager();
    Assert(pPWM != NULL);

    PlayerCreature* pWaitingPlayer = pPWM->getWaitingPartner(pPC);

    uint result = pPWM->acceptPartner(pPC);
    if (result != 0) {
        gcNPCResponse.setCode(NPC_RESPONSE_COUPLE_CANNOT_MEET);
        gcNPCResponse.setParameter(result);
    } else {
        gcNPCResponse.setCode(NPC_RESPONSE_COUPLE_MEET_SUCCESS);
    }

    pPC->getPlayer()->sendPacket(&gcNPCResponse);

    if (pWaitingPlayer != NULL)
        pWaitingPlayer->getPlayer()->sendPacket(&gcNPCResponse);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionAcceptCoupleRequest::toString() const

{
    StringStream msg;
    msg << "ActionAcceptCoupleRequest(" << ")";

    return msg.toString();
}
