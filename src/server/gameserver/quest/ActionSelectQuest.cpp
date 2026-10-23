////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSelectQuest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSelectQuest.h"

#include "Creature.h"
#include "GCMonsterKillQuestInfo.h"
#include "GCNPCResponse.h"
#include "GCSelectQuestID.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "StringPool.h"
#include "VariableManager.h"
#include "mission/MonsterKillQuestInfo.h"
#include "mission/QuestInfoManager.h"
#include "mission/QuestManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionSelectQuest::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionSelectQuest::execute(Creature* pCreature1, Creature* pCreature2)

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

    list<QuestID_t> quests;
    pNPC->getQuestInfoManager()->getPossibleQuestIDs(pPC, back_inserter(quests));

    if (context().variables().bSendQuestInfo()) {
        GCMonsterKillQuestInfo gcMKQInfo;
        list<QuestInfo*> QIs;
        pNPC->getQuestInfoManager()->getMonsterKillQuests(quests.begin(), quests.end(), back_inserter(QIs));

        list<QuestInfo*>::iterator itr = QIs.begin();
        list<QuestInfo*>::iterator endItr = QIs.end();

        for (; itr != endItr; ++itr) {
            MonsterKillQuestInfo* pQI = dynamic_cast<MonsterKillQuestInfo*>(*itr);
            if (pQI == NULL)
                continue;

            GCMonsterKillQuestInfo::QuestInfo* pPacketQI = new GCMonsterKillQuestInfo::QuestInfo;

            pPacketQI->questID = pQI->getQuestID();
            pPacketQI->sType = pQI->getTargetMonsterSpriteType();
            pPacketQI->goal = pQI->getGoalNum();
            pPacketQI->timeLimit = pQI->getTimeLimit();


            gcMKQInfo.addQuestInfo(pPacketQI);
        }

        pPlayer->sendPacket(&gcMKQInfo);
    }

    GCSelectQuestID gcSelectQuestID(quests.begin(), quests.end());

    pPlayer->sendPacket(&gcSelectQuestID);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSelectQuest::toString() const

{
    StringStream msg;
    msg << "ActionSelectQuest(" << ")";

    return msg.toString();
}
