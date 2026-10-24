////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSayDynamic.cpp
// Written By  : excel96
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSayDynamic.h"

#include "Creature.h"
#include "GCNPCSayDynamic.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Script.h"
#include "ScriptManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionSayDynamic::read(PropertyBuffer& propertyBuffer)

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
void ActionSayDynamic::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    const Script* pScript = context().publicScripts().getScript(m_ScriptID);

    GCNPCSayDynamic gcNPCSayDynamic;
    gcNPCSayDynamic.setObjectID(pCreature1->getObjectID());
    gcNPCSayDynamic.setMessage(pScript->getSubject(0));

    Player* pPlayer = pCreature2->getPlayer();
    pPlayer->sendPacket(&gcNPCSayDynamic);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSayDynamic::toString() const

{
    StringStream msg;
    msg << "ActionSayDynamic(" << ",ScriptID:" << (int)m_ScriptID << ")";

    return msg.toString();
}
