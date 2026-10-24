////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionClearBloodBible.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionClearBloodBible.h"

#include "BloodBibleBonus.h"
#include "BloodBibleBonusManager.h"
#include "Creature.h"
#include "GCBloodBibleSignInfo.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "StringPool.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionClearBloodBible::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionClearBloodBible::execute(Creature* pCreature1, Creature* pCreature2)

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

    BloodBibleSignInfo* pInfo = pPC->getBloodBibleSign();
    pInfo->getList().clear();
    GCBloodBibleSignInfo gcInfo;
    gcInfo.setSignInfo(pInfo);
    pPlayer->sendPacket(&gcInfo);

    pPC->initAllStatAndSend();
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionClearBloodBible::toString() const

{
    StringStream msg;
    msg << "ActionClearBloodBible(" << ")";

    return msg.toString();
}
