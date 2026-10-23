
#include "SimpleQuestRewardManager.h"

#include "DB.h"
#include "Item.h"
#include "ItemRewardInfo.h"
#include "RandomRewardClass.h"
#include "SlayerWeaponRewardClass.h"
#include "repository/QuestInfoRepository.h"

void SimpleQuestRewardManager::load(const string& name) {
    vector<ItemRewardRow> itemRewards = defaultQuestInfoRepository().loadItemRewardsOfNPC(name);

    for (size_t r = 0; r < itemRewards.size(); r++) {
        RewardClass_t rClass = (RewardClass_t)itemRewards[r].rewardClass;
        RewardID_t rID = (RewardID_t)itemRewards[r].rewardID;
        Item::ItemClass iClass = (Item::ItemClass)itemRewards[r].itemClass;
        ItemType_t iType = (ItemType_t)itemRewards[r].itemType;
        string option = itemRewards[r].optionType;
        DWORD time = (DWORD)itemRewards[r].timeLimitSec;

        if (m_RewardClasses[rClass] == NULL) {
            m_RewardClasses[rClass] = new RandomRewardClass(rClass);
        }

        ItemRewardInfo* pItemRI = new ItemRewardInfo(rID, rClass, iClass, iType, option, time);
        m_RewardClasses[rClass]->addRewardInfo(pItemRI);
    }

    vector<ItemRewardRow> weaponRewards = defaultQuestInfoRepository().loadSlayerWeaponRewardsOfNPC(name);

    for (size_t r = 0; r < weaponRewards.size(); r++) {
        RewardClass_t rClass = (RewardClass_t)weaponRewards[r].rewardClass;
        RewardID_t rID = (RewardID_t)weaponRewards[r].rewardID;
        Item::ItemClass iClass = (Item::ItemClass)weaponRewards[r].itemClass;
        ItemType_t iType = (ItemType_t)weaponRewards[r].itemType;
        string option = weaponRewards[r].optionType;
        DWORD time = (DWORD)weaponRewards[r].timeLimitSec;

        if (m_RewardClasses[rClass] == NULL) {
            m_RewardClasses[rClass] = new SlayerWeaponRewardClass(rClass);
        }

        ItemRewardInfo* pItemRI = new ItemRewardInfo(rID, rClass, iClass, iType, option, time);
        m_RewardClasses[rClass]->addRewardInfo(pItemRI);
    }
}
