////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionStashOpen.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionStashOpen.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionStashOpen::read(PropertyBuffer& propertyBuffer)

{}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionStashOpen::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    // If the player has not been assigned the stash-related OIDs in this
    // zone yet, assign them here.
    if (pCreature2->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
        if (!pSlayer->getStashStatus())
            pSlayer->registerStash();
    } else if (pCreature2->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
        if (!pVampire->getStashStatus())
            pVampire->registerStash();
    } else if (pCreature2->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature2);
        if (!pOusters->getStashStatus())
            pOusters->registerStash();
    }

    GCNPCResponse okpkt;
    okpkt.setCode(NPC_RESPONSE_INTERFACE_STASHOPEN);
    pCreature2->getPlayer()->sendPacket(&okpkt);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
string ActionStashOpen::toString() const

{
    StringStream msg;
    msg << "ActionStashOpen(" << ")";
    return msg.toString();
}
