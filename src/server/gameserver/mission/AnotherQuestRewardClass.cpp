#include "AnotherQuestRewardClass.h"

#include "Assert.h"
#include "RewardInfo.h"

QuestMessage AnotherQuestRewardClass::giveReward(PlayerCreature* pPC) const

{
    Assert(pPC != NULL);

    vector<RewardInfo*>::size_type index = rand() % m_RewardInfos.size();
    return m_RewardInfos[index]->giveReward(pPC);
}

string AnotherQuestRewardClass::toString() const {
    return "AnotherQuestRewardClass() : \n" + RewardClass::toString();
}
