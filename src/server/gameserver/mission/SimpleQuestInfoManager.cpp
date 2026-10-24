
#include "SimpleQuestInfoManager.h"

#include "Assert.h"
#include "DB.h"
#include "MonsterKillQuestInfo.h"
#include "RewardClass.h"
#include "RewardClassInfoManager.h"
#include "repository/QuestInfoRepository.h"

void SimpleQuestInfoManager::load(const string& name) {
    clear();

    vector<MonsterKillQuestRow> rows = defaultQuestInfoRepository().loadMonsterKillQuestsOfNPC(name);

    for (size_t r = 0; r < rows.size(); r++) {
        QuestID_t qID = (QuestID_t)rows[r].head.questID;
        Race_t race = (Race_t)rows[r].head.race;
        QuestGrade_t MaxGrade = (QuestGrade_t)rows[r].head.maxGrade;
        QuestGrade_t MinGrade = (QuestGrade_t)rows[r].head.minGrade;
        DWORD timeLimit = (DWORD)rows[r].head.timeLimitSec;
        RewardClass_t rewardClass = (RewardClass_t)rows[r].head.rewardClass;
        SpriteType_t monsterSType = (SpriteType_t)rows[r].targetSType;
        bool isChief = (rows[r].isChief == 0) ? false : true;
        int killCount = (int)rows[r].goal;


        MonsterKillQuestInfo* pMonsterKillQI = new MonsterKillQuestInfo(qID, race, MaxGrade, MinGrade, timeLimit,
                                                                        rewardClass, monsterSType, isChief, killCount);
        addQuestInfo(pMonsterKillQI);
    }
}
