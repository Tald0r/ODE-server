////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionGiveEventQuestScript.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionGiveEventQuestScript.h"

#include "Creature.h"
#include "GCNPCAsk.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "mission/EventQuestAdvance.h"
#include "mission/QuestManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionGiveEventQuestScript::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_ScriptID[0] = propertyBuffer.getPropertyInt("Level1");
        m_ScriptID[1] = propertyBuffer.getPropertyInt("Level2");
        m_ScriptID[2] = propertyBuffer.getPropertyInt("Level3");
        m_ScriptID[3] = propertyBuffer.getPropertyInt("Level4");
        m_ScriptID[4] = propertyBuffer.getPropertyInt("Level5");
        m_NotComplete = propertyBuffer.getPropertyInt("NotComplete");
        m_Success = propertyBuffer.getPropertyInt("Success");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionGiveEventQuestScript::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);
    int qLevel = pPC->getQuestManager()->getEventQuestAdvanceManager()->getQuestLevel();
    ScriptID_t sID = m_ScriptID[qLevel];

    if (!pPC->getQuestManager()->canStartMoreQuest()) {
        QuestID_t qID;
        if (pPC->getQuestManager()->successEventQuest(qLevel, qID)) {
            sID = m_Success;
        } else {
            sID = m_NotComplete;
        }
    }

    GCNPCAsk gcNPCAsk;
    gcNPCAsk.setObjectID(pCreature1->getObjectID());
    gcNPCAsk.setScriptID(sID);
    gcNPCAsk.setNPCID(dynamic_cast<NPC*>(pCreature1)->getNPCID());

    Player* pPlayer = pCreature2->getPlayer();
    pPlayer->sendPacket(&gcNPCAsk);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionGiveEventQuestScript::toString() const

{
    StringStream msg;
    msg << "ActionGiveEventQuestScript(" << ")";

    return msg.toString();
}
