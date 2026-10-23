////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionAskByQuestLevel.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionAskByQuestLevel.h"

#include "Creature.h"
#include "GCNPCAsk.h"
#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "mission/EventQuestAdvance.h"
#include "mission/QuestManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionAskByQuestLevel::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_ScriptID[0] = propertyBuffer.getPropertyInt("Level1");
        m_ScriptID[1] = propertyBuffer.getPropertyInt("Level2");
        m_ScriptID[2] = propertyBuffer.getPropertyInt("Level3");
        m_ScriptID[3] = propertyBuffer.getPropertyInt("Level4");
        m_ScriptID[4] = propertyBuffer.getPropertyInt("Level5");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionAskByQuestLevel::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    ScriptID_t sID = m_ScriptID[pPC->getQuestManager()->getEventQuestAdvanceManager()->getQuestLevel()];

    if (sID == 0) {
        GCNPCResponse gcNPCResponse;
        gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        Player* pPlayer = pCreature2->getPlayer();
        pPlayer->sendPacket(&gcNPCResponse);
    } else {
        GCNPCAsk gcNPCAsk;
        gcNPCAsk.setObjectID(pCreature1->getObjectID());
        gcNPCAsk.setScriptID(sID);
        gcNPCAsk.setNPCID(dynamic_cast<NPC*>(pCreature1)->getNPCID());

        Player* pPlayer = pCreature2->getPlayer();
        pPlayer->sendPacket(&gcNPCAsk);
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionAskByQuestLevel::toString() const

{
    StringStream msg;
    msg << "ActionAskByQuestLevel(" << ")";

    return msg.toString();
}
