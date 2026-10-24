#include "RewardClass.h"

#include "Assert.h"
#include "RewardInfo.h"
#include "StringStream.h"

RewardClass::RewardClass(RewardClass_t rClass) {
    m_RewardClass = rClass;
    m_RewardInfos.clear();
}

RewardClass::~RewardClass()

{
    vector<RewardInfo*>::iterator itr = m_RewardInfos.begin();
    vector<RewardInfo*>::iterator endItr = m_RewardInfos.end();

    for (; itr != endItr; ++itr) {
        if (*itr != NULL)
            SAFE_DELETE(*itr);
    }

    m_RewardInfos.clear();
}

void RewardClass::addRewardInfo(RewardInfo* pRewardInfo)

{
    Assert(pRewardInfo != NULL);
    m_RewardInfos.push_back(pRewardInfo);
}

QuestMessage RewardClass::giveReward(PlayerCreature* pPC) const {
    RewardInfo* pRI = selectReward(pPC);
    return pRI->giveReward(pPC);
}

QuestMessage RewardClass::canGiveReward(PlayerCreature* pPC) const

{
    Assert(pPC != NULL);

    vector<RewardInfo*>::const_iterator itr = m_RewardInfos.begin();
    vector<RewardInfo*>::const_iterator endItr = m_RewardInfos.end();

    for (; itr != endItr; ++itr) {
        if (*itr != NULL) {
            QuestMessage result = (*itr)->canGiveReward(pPC);

            if (result != COMPLETE_SUCCESS)
                return result;
        }
    }

    return COMPLETE_SUCCESS;
}

string RewardClass::toString() const {
    StringStream msg;

    msg << "RewardClass(" << "RewardClass# : " << m_RewardClass;

    vector<RewardInfo*>::const_iterator itr = m_RewardInfos.begin();
    vector<RewardInfo*>::const_iterator endItr = m_RewardInfos.end();

    for (; itr != endItr; ++itr) {
        msg << ", " << (*itr)->toString();
    }

    msg << ")";

    return msg.toString();
}
