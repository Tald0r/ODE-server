////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionAsk.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionAsk.h"

#include "Creature.h"
#include "GCNPCAsk.h"
#include "GamePlayer.h"
#include "NPC.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionAsk::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_ScriptID = propertyBuffer.getPropertyInt("ScriptID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionAsk::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    GCNPCAsk gcNPCAsk;
    gcNPCAsk.setObjectID(pCreature1->getObjectID());
    gcNPCAsk.setScriptID(m_ScriptID);
    gcNPCAsk.setNPCID(dynamic_cast<NPC*>(pCreature1)->getNPCID());

    Player* pPlayer = pCreature2->getPlayer();
    pPlayer->sendPacket(&gcNPCAsk);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionAsk::toString() const

{
    StringStream msg;
    msg << "ActionAsk(" << ",ScriptID:" << (int)m_ScriptID << ")";

    return msg.toString();
}
