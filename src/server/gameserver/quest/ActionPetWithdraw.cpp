////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionPetWithdraw.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionPetWithdraw.h"

#include "GCPetStashList.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionPetWithdraw::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionPetWithdraw::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    GCPetStashList gcPetStashList;
    makeGCPetStashList(&gcPetStashList, pPC);

    pPC->getPlayer()->sendPacket(&gcPetStashList);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionPetWithdraw::toString() const

{
    StringStream msg;
    msg << "ActionPetWithdraw(" << ")";

    return msg.toString();
}
