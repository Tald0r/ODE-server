#include "EventQuestLootingManager.h"

#include <iosfwd>
#include <list>

#include "DB.h"
#include "EventQuestAdvance.h"
#include "GameContext.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "Monster.h"
#include "Ousters.h"
#include "PlayerCreature.h"
#include "QuestManager.h"
#include "Slayer.h"
#include "StringStream.h"
#include "Vampire.h"
#include "repository/QuestInfoRepository.h"

int g_ratio[5] = {34, 50, 34, 25, 16};

string EventQuestLootingInfo::toString() const {
    StringStream msg;
    msg << "EventQuestLootingInfo : " << "QuestLevel : " << m_QuestLevel << " ,Type : " << (uint)m_Type
        << "\n\tZoneID : " << m_LootingZoneID << " ,MonsterType : " << m_LootingMonsterType
        << "\n\tItemClass : " << ItemClass2String[m_LootingItemClass].c_str()
        << " ,MinItemType : " << m_LootingItemTypeMin << " ,MaxItemType : " << m_LootingItemTypeMax
        << "\n\tRace : " << (uint)m_Race << " ,Min Grade : " << m_MinGrade << " ,Max Grade : " << m_MaxGrade;

    return msg.toString();
}

bool EventQuestLootingInfo::isTargetMonster(PlayerCreature* pPC, Monster* pMonster) const {
    if (pPC->getRace() == m_Race) {
        QuestID_t qID;
        if (!pPC->getQuestManager()->hasEventQuest(m_QuestLevel, qID))
            return false;

        QuestGrade_t qGrade;
        if (pPC->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);

            qGrade = pSlayer->getQuestGrade();
        } else if (pPC->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pPC);

            qGrade = pVampire->getLevel();
        } else if (pPC->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);

            qGrade = pOusters->getLevel();
        } else {
            cout << "Unknown race!" << endl;
            qGrade = 0;
        }

        if (qGrade < m_MinGrade || qGrade > m_MaxGrade)
            return false;

        if (m_Type == LOOTING_ZONE || m_Type == LOOTING_BOTH) {
            if (pMonster->getZoneID() != m_LootingZoneID)
                return false;
        }

        if (m_Type == LOOTING_MONSTER || m_Type == LOOTING_BOTH) {
            if (pMonster->getMonsterType() != m_LootingMonsterType)
                return false;
        }

        return true;
    } else {
        return false;
    }
}

Item* EventQuestLootingInfo::getLootingItem() {
    int diff = m_LootingItemTypeMax - m_LootingItemTypeMin + 1;
    Assert(diff > 0);

    int offset = rand() % diff;
    ItemType_t itemType = m_LootingItemTypeMin + offset;

    list<OptionType_t> oList;
    Item* pRet = de::gameContext().itemFactories().createItem(m_LootingItemClass, itemType, oList);

    pRet->setQuestItem();

    return pRet;
}

EventQuestLootingInfo* EventQuestLootingManager::isTargetMonster(PlayerCreature* pPC, Monster* pMonster) {
    {
        vector<EventQuestLootingInfo*>& infos = m_ZoneLootingInfo[pMonster->getZoneID()];
        vector<EventQuestLootingInfo*>::const_iterator itr = infos.begin();
        vector<EventQuestLootingInfo*>::const_iterator endItr = infos.end();

        for (; itr != endItr; ++itr) {
            if ((*itr)->isTargetMonster(pPC, pMonster)) {
                return (*itr);
            }
        }
    }

    {
        const vector<EventQuestLootingInfo*>& infos = m_MonsterLootingInfo[pMonster->getMonsterType()];
        vector<EventQuestLootingInfo*>::const_iterator itr = infos.begin();
        vector<EventQuestLootingInfo*>::const_iterator endItr = infos.end();

        for (; itr != endItr; ++itr) {
            if ((*itr)->isTargetMonster(pPC, pMonster)) {
                return (*itr);
            }
        }
    }

    return NULL;
}

bool EventQuestLootingManager::killed(PlayerCreature* pPC, Monster* pMonster) {
    if (!pPC->getQuestManager()->hasQuest())
        return false;

    EventQuestLootingInfo* pInfo = isTargetMonster(pPC, pMonster);
    if (pInfo == NULL)
        return false;

    if ((rand() % 100) < g_ratio[pInfo->m_QuestLevel]) {
        Item* pItem = pInfo->getLootingItem();
        if (pItem == NULL)
            return false;

        pMonster->setQuestItem(pItem);
        return true;
    }

    return false;
}

void EventQuestLootingManager::load() {
    vector<EventQuestLootingRow> rows = defaultQuestInfoRepository().loadEventQuestLootingInfos();

    for (size_t r = 0; r < rows.size(); r++) {
        EventQuestLootingInfo* pInfo = new EventQuestLootingInfo;

        pInfo->m_QuestLevel = rows[r].questLevel;
        pInfo->m_Type = (EventQuestLootingInfo::TYPE)rows[r].lootingType;
        pInfo->m_LootingZoneID = (ZoneID_t)rows[r].lootingZone;
        pInfo->m_LootingMonsterType = (MonsterType_t)rows[r].lootingMType;
        pInfo->m_LootingItemClass = (Item::ItemClass)rows[r].lootingIClass;
        pInfo->m_LootingItemTypeMin = (ItemType_t)rows[r].lootingITypeMin;
        pInfo->m_LootingItemTypeMax = (ItemType_t)rows[r].lootingITypeMax;
        pInfo->m_Race = (Race_t)rows[r].race;
        pInfo->m_MinGrade = (QuestGrade_t)rows[r].minGrade;
        pInfo->m_MaxGrade = (QuestGrade_t)rows[r].maxGrade;

        if (pInfo->m_Type == EventQuestLootingInfo::LOOTING_ZONE)
            m_ZoneLootingInfo[pInfo->m_LootingZoneID].push_back(pInfo);
        else if (pInfo->m_Type != EventQuestLootingInfo::LOOTING_NONE)
            m_MonsterLootingInfo[pInfo->m_LootingMonsterType].push_back(pInfo);
    }
}
