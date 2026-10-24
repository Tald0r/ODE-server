////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionRandomSay.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionRandomSay.h"

#include "Creature.h"
#include "GCNPCSay.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "Script.h"
#include "ScriptManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionRandomSay::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_StartScriptID = propertyBuffer.getPropertyInt("StartScriptID");
        m_EndScriptID = propertyBuffer.getPropertyInt("EndScriptID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionRandomSay::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_DEBUG

    Assert(pCreature1 != NULL);
    Assert(pCreature2 == NULL);
    Assert(pCreature1->isNPC());

    // RandomSay picks one script at random from the range between Start and
    // End and sends it to the client, as the code below shows.
    // A script id missing between Start and End is therefore a problem:
    // when the script table is built, the entries used by RandomSay
    // must be stored contiguously.
    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    ScriptID_t scriptID = m_StartScriptID + random() % (m_EndScriptID - m_StartScriptID + 1);
    const Script* pScript = context().publicScripts().getScript(scriptID);

    GCNPCSay gcNPCSay;
    gcNPCSay.setObjectID(pNPC->getObjectID());
    gcNPCSay.setScriptID(pScript->getScriptID());
    gcNPCSay.setSubjectID(0);

    Zone* pZone = pNPC->getZone();
    Assert(pZone != NULL);
    pZone->broadcastPacket(pNPC->getX(), pNPC->getY(), &gcNPCSay);

    __END_DEBUG
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionRandomSay::toString() const

{
    StringStream msg;
    msg << "ActionRandomSay(" << ",ScriptID:" << (int)m_StartScriptID << "-" << (int)m_EndScriptID << ")";

    return msg.toString();
}
