#include "RandomRewardClass.h"

#include "Assert.h"
#include "RewardInfo.h"

RewardInfo* RandomRewardClass::selectReward(PlayerCreature* pPC) const {
    Assert(pPC != NULL);

    vector<RewardInfo*>::size_type index = rand() % m_RewardInfos.size();
    return m_RewardInfos[index];
}

string RandomRewardClass::toString() const {
    return "RandomRewardClass() : \n" + RewardClass::toString();
}
