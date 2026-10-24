////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionEventMeet.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionEventMeet.h"

#include "Creature.h"
#include "GCNPCAsk.h"
#include "GCNoticeEvent.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "mission/QuestManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionEventMeet::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_ScriptID = propertyBuffer.getPropertyInt("ScriptID");
        m_CounterScriptID = propertyBuffer.getPropertyInt("CounterScriptID");
        m_bGiveInfo = propertyBuffer.getPropertyInt("GiveInfo") != 0;
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionEventMeet::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);

    if (pPC->getQuestManager()->isTargetNPC(pNPC)) {
        if (m_bGiveInfo)
            pPC->getQuestManager()->metNPC(pNPC);

        Player* pPlayer = pCreature2->getPlayer();

        GCNPCAsk gcNPCAsk;
        gcNPCAsk.setObjectID(pCreature1->getObjectID());
        gcNPCAsk.setScriptID(m_ScriptID);
        gcNPCAsk.setNPCID(dynamic_cast<NPC*>(pCreature1)->getNPCID());

        pPlayer->sendPacket(&gcNPCAsk);

        pPC->sendCurrentQuestInfo();
    } else {
        GCNPCAsk gcNPCAsk;
        gcNPCAsk.setObjectID(pCreature1->getObjectID());
        gcNPCAsk.setScriptID(m_CounterScriptID);
        gcNPCAsk.setNPCID(dynamic_cast<NPC*>(pCreature1)->getNPCID());

        Player* pPlayer = pCreature2->getPlayer();
        pPlayer->sendPacket(&gcNPCAsk);
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionEventMeet::toString() const

{
    StringStream msg;
    msg << "ActionEventMeet(" << ",ScriptID:" << (int)m_ScriptID << ")";

    return msg.toString();
}
