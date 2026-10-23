////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionGiveLotto.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionGiveLotto.h"

#include "CGLotterySelect.h"
#include "Creature.h"
#include "GCDeleteInventoryItem.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "mission/EventQuestAdvance.h"
#include "mission/QuestManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionGiveLotto::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_QuestLevel = propertyBuffer.getPropertyInt("QuestLevel") - 1;
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionGiveLotto::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    QuestID_t qID;
    int questLevel = m_QuestLevel;

    if (questLevel < 0)
        questLevel = pPC->getQuestManager()->getEventQuestAdvanceManager()->getQuestLevel() - 1;

    cout << "Activated : " << toString() << " ... " << pPC->getName() << " ...  " << questLevel << endl;

    EventQuestAdvance::Status status = pPC->getQuestManager()->getEventQuestAdvanceManager()->getStatus(questLevel);
    int ownerQuestLevel = pPC->getQuestManager()->getEventQuestAdvanceManager()->getQuestLevel();

    if ((ownerQuestLevel > questLevel && status == EventQuestAdvance::EVENT_QUEST_ADVANCED) ||
        ((questLevel == 4 && ownerQuestLevel == -1) || pPC->getQuestManager()->successEventQuest(questLevel, qID))) {
        GamePlayer* pGP = dynamic_cast<GamePlayer*>(pPC->getPlayer());
        Assert(pGP != NULL);

        if (true) {
            pPC->getQuestManager()->getEventQuestAdvanceManager()->rewarded(questLevel);
            pPC->getQuestManager()->getEventQuestAdvanceManager()->save();
            if (questLevel != 4)
                pPC->getQuestManager()->questRewarded(qID);
            else
                pPC->getQuestManager()->cancelQuest();

            pPC->setLottoQuestLevel(questLevel);

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

            CGLotterySelect cgLS;
            cgLS.setType(TYPE_FINISH_SCRATCH);
            cgLS.setQuestLevel(0);
            cgLS.setGiftID(0);
            CGLotterySelectHandler::execute(&cgLS, pPC->getPlayer());

        } else {
            Player* pPlayer = pCreature2->getPlayer();

            GCNPCResponse gcNPCResponse;
            gcNPCResponse.setCode(NPC_RESPONSE_LOTTERY);
            gcNPCResponse.setParameter((uint)questLevel);

            pPlayer->sendPacket(&gcNPCResponse);

            filelog("EventQuest.log", "ActionGiveLotto : %d to %s", questLevel, pPC->getName().c_str());
        }
    }

    GCNPCResponse gcNPCResponse;
    gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPC->getPlayer()->sendPacket(&gcNPCResponse);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionGiveLotto::toString() const

{
    StringStream msg;
    msg << "ActionGiveLotto(" << "QuestLevel : " << m_QuestLevel << ")";

    return msg.toString();
}
