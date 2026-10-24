////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSay.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSay.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GCNPCSay.h"
#include "GamePlayer.h"
#include "NPC.h"

////////////////////////////////////////////////////////////////////////////////
// read from property buffer
////////////////////////////////////////////////////////////////////////////////
void ActionSay::read(PropertyBuffer& propertyBuffer)

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
void ActionSay::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCNPCResponse okpkt;
    pPlayer->sendPacket(&okpkt);

    GCNPCSay gcNPCSay;
    gcNPCSay.setObjectID(pNPC->getObjectID());
    gcNPCSay.setScriptID(m_ScriptID);
    gcNPCSay.setSubjectID(0);
    pPlayer->sendPacket(&gcNPCSay);
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSay::toString() const

{
    StringStream msg;
    msg << "ActionSay(" << ",ScriptID:" << (int)m_ScriptID << ")";
    return msg.toString();
}
