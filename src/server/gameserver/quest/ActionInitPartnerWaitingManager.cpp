////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionInitPartnerWaitingManager.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionInitPartnerWaitingManager.h"

#include "Creature.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "SystemAvailabilitiesManager.h"
#include "couple/PartnerWaitingManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionInitPartnerWaitingManager::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionInitPartnerWaitingManager::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature1->isNPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_COUPLE);

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);

    pNPC->setCoupleRegisterManager(new PartnerWaitingManager(WAIT_FOR_MEET));
    pNPC->setCoupleUnregisterManager(new PartnerWaitingManager(WAIT_FOR_APART));
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionInitPartnerWaitingManager::toString() const

{
    StringStream msg;
    msg << "ActionInitPartnerWaitingManager(" << ")";

    return msg.toString();
}
