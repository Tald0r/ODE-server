
#include "RewardClassInfoManager.h"

#include "ItemRewardInfo.h"
#include "RandomRewardClass.h"
#include "SlayerWeaponRewardClass.h"

RewardClassInfoManager::RewardClassInfoManager(NPC* pNPC) {
    m_RewardClasses.clear();
    m_pOwnerNPC = pNPC;
}

RewardClassInfoManager::~RewardClassInfoManager() {
    RewardClassHashMap::iterator itr = m_RewardClasses.begin();
    RewardClassHashMap::iterator endItr = m_RewardClasses.end();

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL)
            SAFE_DELETE(itr->second);
    }

    m_RewardClasses.clear();
}

QuestMessage RewardClassInfoManager::canGiveReward(RewardClass_t rClass, PlayerCreature* pPC) const {
    RewardClass* pRewardClass = getRewardClass(rClass);

    if (pRewardClass == NULL)
        return COMPLETE_FAIL_INVALID_NPC;

    return pRewardClass->canGiveReward(pPC);
}

RewardClass* RewardClassInfoManager::getRewardClass(RewardClass_t rClass) const {
    RewardClassHashMap::const_iterator itr = m_RewardClasses.find(rClass);

    if (itr == m_RewardClasses.end())
        return NULL;

    return itr->second;
}
