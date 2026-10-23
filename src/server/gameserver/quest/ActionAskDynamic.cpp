////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionAskDynamic.cpp
// Written By  : excel96
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionAskDynamic.h"

#include "Creature.h"
#include "GCNPCAskDynamic.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Script.h"
#include "ScriptManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionAskDynamic::read(PropertyBuffer& propertyBuffer)

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
void ActionAskDynamic::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    const Script* pScript = context().publicScripts().getScript(m_ScriptID);

    GCNPCAskDynamic gcNPCAskDynamic;
    gcNPCAskDynamic.setObjectID(pCreature1->getObjectID());
    gcNPCAskDynamic.setScriptID(m_ScriptID);
    gcNPCAskDynamic.setSubject(pScript->getSubject(0));

    for (uint c = 0; c < pScript->getContentCount(); c++) {
        gcNPCAskDynamic.addContent(pScript->getContent(c));
    }

    Player* pPlayer = pCreature2->getPlayer();
    pPlayer->sendPacket(&gcNPCAskDynamic);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionAskDynamic::toString() const

{
    StringStream msg;
    msg << "ActionAskDynamic(" << ",ScriptID:" << (int)m_ScriptID << ")";

    return msg.toString();
}
