////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionPetDeposit.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionPetDeposit.h"

#include "ActionQuitDialogue.h"
#include "GCNPCResponse.h"
#include "GCPetStashList.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionPetDeposit::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionPetDeposit::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    GCPetStashList gcPetStashList;
    makeGCPetStashList(&gcPetStashList, pPC);

    /* Close the dialogue window before the pet stash opens. */
    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPC->getPlayer()->sendPacket(&response);

    pPC->getPlayer()->sendPacket(&gcPetStashList);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionPetDeposit::toString() const

{
    StringStream msg;
    msg << "ActionPetDeposit(" << ")";

    return msg.toString();
}
