////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionGiveEventQuest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionGiveEventQuest.h"

#include <algorithm>
#include <iosfwd>

#include "Creature.h"
#include "GCMonsterKillQuestInfo.h"
#include "GCNPCResponse.h"
#include "GCSelectQuestID.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "VariableManager.h"
#include "mission/EventQuestAdvance.h"
#include "mission/MonsterKillQuestInfo.h"
#include "mission/QuestInfoManager.h"
#include "mission/QuestManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionGiveEventQuest::read(PropertyBuffer& propertyBuffer)

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
void ActionGiveEventQuest::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature1->isNPC());

    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    if (!pPC->getQuestManager()->canStartMoreQuest()) {
        GCNPCResponse gcNPCResponse;
        gcNPCResponse.setCode(NPC_RESPONSE_QUEST);
        gcNPCResponse.setParameter((uint)START_FAIL_QUEST_NUM_EXCEEDED);

        pPlayer->sendPacket(&gcNPCResponse);
        return;
    }

    if (pPC->getQuestManager()->getEventQuestAdvanceManager()->getQuestLevel() != m_QuestLevel) {
        GCNPCResponse gcNPCResponse;
        gcNPCResponse.setCode(NPC_RESPONSE_QUEST);
        gcNPCResponse.setParameter((uint)START_FAIL_PC);

        pPlayer->sendPacket(&gcNPCResponse);
        return;
    }

    list<QuestID_t> quests;
    pNPC->getQuestInfoManager()->getEventQuestIDs(m_QuestLevel, pPC, back_inserter(quests));


    GCSelectQuestID gcGiveEventQuestID(quests.begin(), quests.end());

    pPlayer->sendPacket(&gcGiveEventQuestID);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionGiveEventQuest::toString() const

{
    StringStream msg;
    msg << "ActionGiveEventQuest(" << ")";

    return msg.toString();
}
