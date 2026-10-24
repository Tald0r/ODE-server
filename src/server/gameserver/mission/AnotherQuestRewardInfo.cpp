
#include "AnotherQuestRewardInfo.h"

#include "PlayerCreature.h"
#include "QuestManager.h"

AnotherQuestRewardInfo::AnotherQuestRewardInfo(RewardID_t rID, RewardClass_t rClass, QuestID_t qID)
    : RewardInfo(rID, rClass), m_QuestID(qID) {}

AnotherQuestRewardInfo::~AnotherQuestRewardInfo() {}

QuestMessage AnotherQuestRewardInfo::canGiveReward(PlayerCreature* pPC) const

{
    return COMPLETE_SUCCESS;
}

QuestMessage AnotherQuestRewardInfo::giveReward(PlayerCreature* pPC) const

{
    QuestMessage result = canGiveReward(pPC);

    if (result != COMPLETE_SUCCESS)
        return result;

    Assert(false);
    return COMPLETE_SUCCESS;
}

string AnotherQuestRewardInfo::toString() const {
    StringStream msg;

    msg << "AnotherQuestRewardInfo(" << "QuestID : " << (uint)m_QuestID << ") : " << "\n"
        << RewardInfo::toString().c_str();

    return msg.toString();
}
