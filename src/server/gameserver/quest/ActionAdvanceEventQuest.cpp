////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionAdvanceEventQuest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionAdvanceEventQuest.h"

#include <algorithm>

#include "Creature.h"
#include "GCDeleteInventoryItem.h"
#include "GCNPCResponse.h"
#include "Inventory.h"
#include "NPC.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "mission/EventQuestAdvance.h"
#include "mission/QuestManager.h"
#include "mission/RewardClassInfoManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionAdvanceEventQuest::read(PropertyBuffer& propertyBuffer)

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
void ActionAdvanceEventQuest::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    QuestMessage result = COMPLETE_FAIL_INVALID_NPC;

    RewardClassInfoManager* pRIM = pNPC->getRewardClassInfoManager();
    Assert(pRIM != NULL);

    if (!pPC->getQuestManager()->hasQuest()) {
        result = COMPLETE_FAIL_NOT_IN_QUEST;
    } else {
        list<RewardClass_t> rList;
        list<QuestID_t> qList;
        pPC->getQuestManager()->getCompletedQuestRewards(back_inserter(qList), back_inserter(rList));
        if (!rList.empty()) {
            QuestID_t qID;

            if (pPC->getQuestManager()->successEventQuest(m_QuestLevel, qID)) {
                if (pPC->getQuestManager()->getEventQuestAdvanceManager()->advanced(m_QuestLevel))
                    pPC->getQuestManager()->getEventQuestAdvanceManager()->save(m_QuestLevel);
                else
                    filelog("EventBug.txt", "ActionAdvanceEventQuest : advanced it, so why does it not take... %s:%d",
                            pPC->getName().c_str(), qID);

                pPC->getQuestManager()->questRewarded(qID);
                result = COMPLETE_SUCCESS;

                list<Item*> iList;
                pPC->getInventory()->clearQuestItem(iList);

                list<Item*>::iterator itr = iList.begin();
                list<Item*>::iterator endItr = iList.end();

                for (; itr != endItr; ++itr) {
                    GCDeleteInventoryItem gcDII;
                    gcDII.setObjectID((*itr)->getObjectID());
                    pPC->getPlayer()->sendPacket(&gcDII);
                    (*itr)->destroy();
                    SAFE_DELETE(*itr);
                }

                iList.clear();
            } else {
                result = COMPLETE_FAIL_INVALID_NPC;
            }

        } else {
            result = COMPLETE_FAIL_NOT_COMPLETE;
        }
    }


    GCNPCResponse gcNPCResponse;

    if (result != COMPLETE_SUCCESS) {
        gcNPCResponse.setCode(NPC_RESPONSE_QUEST);
        gcNPCResponse.setParameter((uint)result);

        pPC->getPlayer()->sendPacket(&gcNPCResponse);

        pPC->getQuestManager()->adjustQuestStatus();
        pPC->sendCurrentQuestInfo();
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionAdvanceEventQuest::toString() const

{
    StringStream msg;
    msg << "ActionAdvanceEventQuest(" << ")";

    return msg.toString();
}
