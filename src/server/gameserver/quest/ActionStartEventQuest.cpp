
////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionStartEventQuest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionStartEventQuest.h"

#include <algorithm>

#include "Creature.h"
#include "GCNPCResponse.h"
#include "NPC.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "mission/EventQuestAdvance.h"
#include "mission/QuestInfoManager.h"
#include "mission/QuestManager.h"
#include "mission/RewardClassInfoManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionStartEventQuest::read(PropertyBuffer& propertyBuffer)

{
    try {
        m_QuestLevel = propertyBuffer.getPropertyInt("QuestLevel") - 1;
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionStartEventQuest::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    GCNPCResponse gcNPCResponse;

    gcNPCResponse.setCode(NPC_RESPONSE_QUEST);

    QuestInfoManager* pQIM = pNPC->getQuestInfoManager();
    if (pQIM == NULL) {
        gcNPCResponse.setParameter(START_FAIL_CANNOT_APPLY_QUEST);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);

        return;
    }

    vector<QuestID_t> qList;
    pQIM->getEventQuestIDs(m_QuestLevel, pPC, back_inserter(qList));
    if (qList.empty()) {
        gcNPCResponse.setParameter(START_FAIL_CANNOT_APPLY_QUEST);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);

        return;
    }

    QuestID_t qID = qList[0];

    pPC->getQuestManager()->adjustQuestStatus();
    pQIM->startQuest(qID, pPC);

    pPC->sendCurrentQuestInfo();

    gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPC->getPlayer()->sendPacket(&gcNPCResponse);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionStartEventQuest::toString() const

{
    StringStream msg;
    msg << "ActionStartEventQuest(" << ")";

    return msg.toString();
}
