////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSetResurrectZone.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSetResurrectZone.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Trigger.h"
#include "Vampire.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionSetResurrectZone::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_ZoneID = propertyBuffer.getPropertyInt("ZoneID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionSetResurrectZone::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    // Send an OK packet to the client first.
    GCNPCResponse okpkt;
    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);
    pPlayer->sendPacket(&okpkt);

    if (pCreature2->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
        pSlayer->setResurrectZoneIDEx(m_ZoneID);
    } else if (pCreature2->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
        pVampire->setResurrectZoneIDEx(m_ZoneID);
    } else if (pCreature2->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature2);
        pOusters->setResurrectZoneIDEx(m_ZoneID);
    }

    GCSystemMessage msg;
    msg.setMessage(context().strings().getString(STRID_SET_RESURRECTION_POSITION));
    pPlayer->sendPacket(&msg);
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSetResurrectZone::toString() const

{
    StringStream msg;
    msg << "ActionSetResurrectZone(" << ")";
    return msg.toString();
}
