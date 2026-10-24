#include "QuestInfoManager.h"

#include "EventQuestAdvance.h"
#include "GameContext.h"
#include "GatherItemQuestInfo.h"
#include "Item.h"
#include "ItemRewardInfo.h"
#include "MonsterKillQuestInfo.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "QuestInfo.h"
#include "QuestManager.h"
#include "VariableManager.h"

QuestInfoManager::~QuestInfoManager()

{
    clear();
}

void QuestInfoManager::clear() {
    QuestInfoHashMap::iterator itr = m_QuestInfos.begin();
    QuestInfoHashMap::iterator endItr = m_QuestInfos.end();

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL)
            SAFE_DELETE(itr->second);
    }

    m_QuestInfos.clear();
}

bool QuestInfoManager::isEventQuest(QuestID_t qID) const {
    QuestInfoHashMap::const_iterator itr = m_QuestInfos.find(qID);
    if (itr != m_QuestInfos.end()) {
        return itr->second->isEventQuest();
    }
    return false;
}

void QuestInfoManager::addQuestInfo(QuestInfo* pQI)

{
    QuestInfoHashMap::iterator itr = m_QuestInfos.find(pQI->getQuestID());

    if (itr != m_QuestInfos.end())
        throw Error("QuestInfoManager::addQuestInfo : duplicate QuestID.");

    m_QuestInfos[pQI->getQuestID()] = pQI;
}

QuestMessage QuestInfoManager::canExecuteQuest(QuestID_t qID, PlayerCreature* pPC) const {
    if (!de::gameContext().variables().canApplyQuest())
        return START_FAIL_CANNOT_APPLY_QUEST;
    if (pPC->getQuestManager()->hasQuest(qID))
        return START_FAIL_DUPLICATED_QUEST_ID;
    if (!pPC->getQuestManager()->canStartMoreQuest())
        return START_FAIL_QUEST_NUM_EXCEEDED;

    QuestInfoHashMap::const_iterator itr = m_QuestInfos.find(qID);
    if (itr == m_QuestInfos.end()) {
        filelog("EventBug.txt", "QuestInfoManager::canExcuteQuest : the qID the client sent is not on this NPC %s:%d",
                m_pOwnerNPC->getName().c_str(), qID);
        return FAIL_BUG;
    }
    if (!itr->second->canExecute(pPC))
        return START_FAIL_PC;

    return START_SUCCESS;
}

QuestMessage QuestInfoManager::startQuest(QuestID_t qID, PlayerCreature* pPC) {
    QuestMessage code = canExecuteQuest(qID, pPC);
    if (code != START_SUCCESS)
        return code;

    QuestStatus* pQuestStatus = makeQuestStatus(qID, pPC);

    if (pQuestStatus->isEventQuest()) {
        if (!pPC->getQuestManager()->getEventQuestAdvanceManager()->start(pQuestStatus->getQuestLevel())) {
            filelog("EventBug.txt",
                    "QuestInfoManager::startQuest : the quest should start, but the quest advance manager is not INIT. "
                    "%s:%d (Level %d)",
                    pPC->getName().c_str(), qID, pQuestStatus->getQuestLevel());
        }
    }

    pPC->getQuestManager()->addQuest(pQuestStatus);

    return code;
}

QuestStatus* QuestInfoManager::makeQuestStatus(QuestID_t qID, PlayerCreature* pPC) const {
    QuestInfoHashMap::const_iterator itr = m_QuestInfos.find(qID);

    if (itr == m_QuestInfos.end())
        return NULL;

    QuestStatus* pQuestStatus = itr->second->makeQuestStatus(pPC);

    pQuestStatus->setEventQuest(itr->second->isEventQuest());
    pQuestStatus->setQuestLevel(itr->second->getQuestLevel());
    pQuestStatus->setTimeLimit(itr->second->getTimeLimit() != 0);

    pQuestStatus->setRewardClass(itr->second->getRewardClass());

    return pQuestStatus;
}
