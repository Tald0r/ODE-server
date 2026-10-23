
#include "QuestManager.h"

#include "EventQuestAdvance.h"
#include "GatherItemQuestStatus.h"
#include "MeetNPCQuestStatus.h"
#include "MiniGameQuestStatus.h"
#include "Monster.h"
#include "MonsterKillQuestStatus.h"
#include "NPC.h"
#include "Packet.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "QuestInfoManager.h"
#include "QuestStatus.h"
#include "StringStream.h"
#include "VariableManager.h"


const unordered_map<QuestID_t, QuestStatus*>::size_type QuestManager::MAX_QUEST_NUM = 1;

QuestManager::QuestManager(PlayerCreature* pOwner) {
    m_pOwner = pOwner;
    m_Quests.clear();

    m_pEventQuestAdvanceManager = new EventQuestAdvanceManager(pOwner);
}

QuestManager::~QuestManager() {
    unordered_map<QuestID_t, QuestStatus*>::iterator itr;
    unordered_map<QuestID_t, QuestStatus*>::iterator endItr = m_Quests.end();

    for (itr = m_Quests.begin(); itr != endItr; ++itr) {
        if (itr->second != NULL)
            SAFE_DELETE(itr->second);
    }

    m_Quests.clear();
}

void QuestManager::load() {
    // Resuming a quest is not supported yet.
    m_pEventQuestAdvanceManager->load();
}

void QuestManager::addQuest(QuestStatus* pQS) {
    Assert(pQS != NULL);
    Assert(!hasQuest(pQS->getQuestID()));
    pQS->setOwnerQM(this);

    m_Quests[pQS->getQuestID()] = pQS;

    pQS->executeWhenStart();
}

QuestMessage QuestManager::isQuestComplete(QuestID_t qID) const {
    unordered_map<QuestID_t, QuestStatus*>::const_iterator itr = m_Quests.find(qID);

    if (itr == m_Quests.end())
        return COMPLETE_FAIL_NOT_IN_QUEST;

    if (itr->second->isSuccess()) {
        return COMPLETE_SUCCESS;
    }

    return COMPLETE_FAIL_NOT_COMPLETE;
}


QuestStatus* QuestManager::getQuestStatus(QuestID_t qID) {
    unordered_map<QuestID_t, QuestStatus*>::iterator itr = m_Quests.find(qID);

    if (itr == m_Quests.end())
        return NULL;
    return itr->second;
}

void QuestManager::sendQuestInfo() {
    unordered_map<QuestID_t, QuestStatus*>::iterator itr = m_Quests.begin();
    unordered_map<QuestID_t, QuestStatus*>::iterator endItr = m_Quests.end();

    for (; itr != endItr; ++itr) {
        if (itr->second == NULL) {
            filelog("Quest.log", "QuestManager::sendQuestInfo : QuestStatus holds a NULL.");
            continue;
        }

        Packet* pPacket = itr->second->makeStatusPacket();

        m_pOwner->getPlayer()->sendPacket(pPacket);

        SAFE_DELETE(pPacket);
    }
}

void QuestManager::adjustQuestStatus() {
    unordered_map<QuestID_t, QuestStatus*>::iterator itr = m_Quests.begin();

    while (itr != m_Quests.end()) {
        if (itr->second != NULL && (itr->second->timeExpired() || itr->second->isRewarded())) {
            SAFE_DELETE(itr->second);
            unordered_map<QuestID_t, QuestStatus*>::iterator prevItr = itr++;

            m_Quests.erase(prevItr);
        } else {
            ++itr;
        }
    }
}

QuestMessage QuestManager::cancelQuest() {
    if (m_Quests.empty()) {
        return CANCEL_NOT_IN_QUEST;
    }

    unordered_map<QuestID_t, QuestStatus*>::iterator itr = m_Quests.begin();
    unordered_map<QuestID_t, QuestStatus*>::iterator endItr = m_Quests.end();
    QuestMessage result = CANCEL_NOT_IN_QUEST;

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL && !itr->second->isRewarded()) {
            itr->second->executeWhenCancel();
            SAFE_DELETE(itr->second);
            m_Quests.erase(itr);
            result = CANCEL_SUCCESS;
            break;
        }
    }

    return result;
}

QuestMessage QuestManager::failQuest() {
    if (m_Quests.empty()) {
        return CANCEL_NOT_IN_QUEST;
    }

    unordered_map<QuestID_t, QuestStatus*>::iterator itr = m_Quests.begin();
    unordered_map<QuestID_t, QuestStatus*>::iterator endItr = m_Quests.end();
    QuestMessage result = CANCEL_NOT_IN_QUEST;

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL && !itr->second->isRewarded()) {
            itr->second->executeWhenFail();
            SAFE_DELETE(itr->second);
            m_Quests.erase(itr);
            result = CANCEL_SUCCESS;
            break;
        }
    }

    return result;
}

bool QuestManager::hasEventQuest(int questLevel, QuestID_t& qID) const {
    unordered_map<QuestID_t, QuestStatus*>::const_iterator itr = m_Quests.begin();
    unordered_map<QuestID_t, QuestStatus*>::const_iterator endItr = m_Quests.end();

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL && itr->second->isEventQuest() && itr->second->getQuestLevel() == questLevel) {
            qID = itr->second->getQuestID();
            return true;
        }
    }

    return false;
}

bool QuestManager::successEventQuest(int questLevel, QuestID_t& qID) const {
    unordered_map<QuestID_t, QuestStatus*>::const_iterator itr = m_Quests.begin();
    unordered_map<QuestID_t, QuestStatus*>::const_iterator endItr = m_Quests.end();

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL && itr->second->isEventQuest() && itr->second->getQuestLevel() == questLevel) {
            qID = itr->second->getQuestID();
            return itr->second->isSuccess();
        }
    }

    return false;
}

// RewardClass_t QuestManager::getEventQuestReward( int questLevel ) const

QuestStatus* QuestManager::getQuestStatusByQuestClass(QuestClass qClass) const

{
    unordered_map<QuestID_t, QuestStatus*>::const_iterator itr = m_Quests.begin();
    unordered_map<QuestID_t, QuestStatus*>::const_iterator endItr = m_Quests.end();

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL && itr->second->getQuestClass() == qClass) {
            return itr->second;
        }
    }

    return NULL;
}

bool QuestManager::killedMonster(Monster* pMonster) {
    MonsterKillQuestStatus* pQS =
        dynamic_cast<MonsterKillQuestStatus*>(getQuestStatusByQuestClass(QUEST_CLASS_MONSTER_KILL));
    if (pQS != NULL) {
        return pQS->killed(pMonster->getSpriteType(), pMonster->isChief());
    }

    return false;
}

bool QuestManager::metNPC(NPC* pNPC) {
    MeetNPCQuestStatus* pQS = dynamic_cast<MeetNPCQuestStatus*>(getQuestStatusByQuestClass(QUEST_CLASS_MEET_NPC));
    if (pQS != NULL) {
        return pQS->met(pNPC->getNPCID());
    }

    return false;
}

bool QuestManager::isTargetNPC(NPC* pNPC) {
    MeetNPCQuestStatus* pQS = dynamic_cast<MeetNPCQuestStatus*>(getQuestStatusByQuestClass(QUEST_CLASS_MEET_NPC));

    if (pQS != NULL) {
        return pQS->isTarget(pNPC->getNPCID());
    }

    return false;
}

bool QuestManager::submitMiniGameScore(int GameType, uint GameScore) {
    MiniGameQuestStatus* pQS = dynamic_cast<MiniGameQuestStatus*>(getQuestStatusByQuestClass(QUEST_CLASS_MINI_GAME));

    if (pQS != NULL && pQS->getGameType() == GameType) {
        return pQS->setScore(GameScore);
    }

    return false;
}

bool QuestManager::completeMonsterKillQuest() {
    MonsterKillQuestStatus* pQS =
        dynamic_cast<MonsterKillQuestStatus*>(getQuestStatusByQuestClass(QUEST_CLASS_MONSTER_KILL));
    if (pQS != NULL) {
        pQS->completeQuest();
        return true;
    }

    return false;
}
