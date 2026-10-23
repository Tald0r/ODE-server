////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSelectBloodBible.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSelectBloodBible.h"

#include "BloodBibleBonus.h"
#include "BloodBibleBonusManager.h"
#include "Creature.h"
#include "GCBloodBibleList.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "StringPool.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionSelectBloodBible::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionSelectBloodBible::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature1->isNPC());

    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCBloodBibleList gcPacket;
    context().bloodBibleBonuses().getBloodBibleByRace(pPC->getRace(), back_inserter(gcPacket.getList()));
    pPlayer->sendPacket(&gcPacket);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSelectBloodBible::toString() const

{
    StringStream msg;
    msg << "ActionSelectBloodBible(" << ")";

    return msg.toString();
}
