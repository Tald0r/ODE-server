////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionStartPetQuest.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionStartPetQuest.h"

#include "Creature.h"
#include "GCMonsterKillQuestInfo.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "MonsterInfo.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "StringPool.h"
#include "VariableManager.h"
#include "mission/MonsterKillQuestStatus.h"
#include "mission/QuestManager.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionStartPetQuest::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionStartPetQuest::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    if (context().variables().getVariable(RACE_PET_QUEST) == 0) {
        GCSystemMessage gcSM;
        gcSM.setMessage(context().strings().getString(STRID_NOT_SUPPORT));

        pPC->getPlayer()->sendPacket(&gcSM);

        return;
    }

    if (!pPC->getQuestManager()->canStartMoreQuest()) {
        GCNPCResponse gcNPCResponse;
        gcNPCResponse.setCode(NPC_RESPONSE_QUEST);
        gcNPCResponse.setParameter((uint)START_FAIL_QUEST_NUM_EXCEEDED);

        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    GCMonsterKillQuestInfo gcMKQI;
    GCMonsterKillQuestInfo::QuestInfo* pQI = pPC->getPetQuestInfo();
    gcMKQI.addQuestInfo(pQI);
    pPC->getPlayer()->sendPacket(&gcMKQI);

    const MonsterInfo* pMonsterInfo = context().monsterInfos().getMonsterInfo(pQI->sType);
    Assert(pMonsterInfo != NULL);

    MonsterKillQuestStatus* pQS =
        new MonsterKillQuestStatus(pQI->questID, VSDateTime::currentDateTime().addSecs(pQI->timeLimit),
                                   pMonsterInfo->getSpriteType(), false, pQI->goal);
    pQS->setTimeLimit(true);

    if (pPC->isSlayer())
        pQS->setRewardClass(70);
    else if (pPC->isVampire())
        pQS->setRewardClass(71);
    else if (pPC->isOusters())
        pQS->setRewardClass(72);

    pPC->getQuestManager()->addQuest(pQS);
    pPC->sendCurrentQuestInfo();
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionStartPetQuest::toString() const

{
    StringStream msg;
    msg << "ActionStartPetQuest(" << ")";

    return msg.toString();
}
