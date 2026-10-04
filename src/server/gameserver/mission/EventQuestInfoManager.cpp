#include "EventQuestInfoManager.h"

#include "Assert.h"
#include "DB.h"
#include "DiagnosticTrace.h"
#include "GatherItemQuestInfo.h"
#include "MeetNPCQuestInfo.h"
#include "MiniGameQuestInfo.h"
#include "MonsterKillQuestInfo.h"
#include "RewardClass.h"
#include "RewardClassInfoManager.h"
#include "repository/QuestInfoRepository.h"

void EventQuestInfoManager::load(const string& name) {
    __BEGIN_TRY

    clear();

    vector<EventMonsterKillQuestRow> monsterKills = defaultQuestInfoRepository().loadEventMonsterKillQuestsOfNPC(name);

    for (size_t r = 0; r < monsterKills.size(); r++) {
        QuestID_t qID = (QuestID_t)monsterKills[r].quest.head.questID;
        Race_t race = (Race_t)monsterKills[r].quest.head.race;
        QuestGrade_t MaxGrade = (QuestGrade_t)monsterKills[r].quest.head.maxGrade;
        QuestGrade_t MinGrade = (QuestGrade_t)monsterKills[r].quest.head.minGrade;
        DWORD timeLimit = (DWORD)monsterKills[r].quest.head.timeLimitSec;
        RewardClass_t rewardClass = (RewardClass_t)monsterKills[r].quest.head.rewardClass;
        SpriteType_t monsterSType = (SpriteType_t)monsterKills[r].quest.targetSType;
        bool isChief = (monsterKills[r].quest.isChief == 0) ? false : true;
        int killCount = (int)monsterKills[r].quest.goal;
        bool isEventQuest = monsterKills[r].eventQuest != 0;
        int questLevel = (int)monsterKills[r].questLevel;


        MonsterKillQuestInfo* pMonsterKillQI = new MonsterKillQuestInfo(qID, race, MaxGrade, MinGrade, timeLimit,
                                                                        rewardClass, monsterSType, isChief, killCount);
        pMonsterKillQI->setEventQuest(isEventQuest);
        pMonsterKillQI->setQuestLevel(questLevel);
        addQuestInfo(pMonsterKillQI);

        de::diagnosticTrace(
            [&](std::ostream& output) { output << "Loading Quest Info : " << pMonsterKillQI->toString(); });
    }

    vector<EventGatherItemQuestRow> gatherItems = defaultQuestInfoRepository().loadEventGatherItemQuestsOfNPC(name);

    for (size_t r = 0; r < gatherItems.size(); r++) {
        QuestID_t qID = (QuestID_t)gatherItems[r].quest.head.questID;
        Race_t race = (Race_t)gatherItems[r].quest.head.race;
        QuestGrade_t MaxGrade = (QuestGrade_t)gatherItems[r].quest.head.maxGrade;
        QuestGrade_t MinGrade = (QuestGrade_t)gatherItems[r].quest.head.minGrade;
        DWORD timeLimit = (DWORD)gatherItems[r].quest.head.timeLimitSec;
        RewardClass_t rewardClass = (RewardClass_t)gatherItems[r].quest.head.rewardClass;
        Item::ItemClass iClass = (Item::ItemClass)gatherItems[r].quest.targetIClass;
        ItemType_t iType = (ItemType_t)gatherItems[r].quest.targetIType;
        int Count = (int)gatherItems[r].quest.goal;
        bool isEventQuest = gatherItems[r].eventQuest != 0;
        int questLevel = (int)gatherItems[r].questLevel;

        GatherItemQuestInfo* pGatherItemQI =
            new GatherItemQuestInfo(qID, race, MaxGrade, MinGrade, timeLimit, rewardClass, iClass, iType, Count);
        pGatherItemQI->setEventQuest(isEventQuest);
        pGatherItemQI->setQuestLevel(questLevel);
        addQuestInfo(pGatherItemQI);

        de::diagnosticTrace(
            [&](std::ostream& output) { output << "Loading Quest Info : " << pGatherItemQI->toString(); });
    }

    vector<EventMeetNPCQuestRow> meetNPCs = defaultQuestInfoRepository().loadEventMeetNPCQuestsOfNPC(name);

    for (size_t r = 0; r < meetNPCs.size(); r++) {
        QuestID_t qID = (QuestID_t)meetNPCs[r].quest.head.questID;
        Race_t race = (Race_t)meetNPCs[r].quest.head.race;
        QuestGrade_t MaxGrade = (QuestGrade_t)meetNPCs[r].quest.head.maxGrade;
        QuestGrade_t MinGrade = (QuestGrade_t)meetNPCs[r].quest.head.minGrade;
        DWORD timeLimit = (DWORD)meetNPCs[r].quest.head.timeLimitSec;
        RewardClass_t rewardClass = (RewardClass_t)meetNPCs[r].quest.head.rewardClass;
        NPCID_t npcID = (NPCID_t)meetNPCs[r].quest.targetNPCID;
        NPCID_t npcID2 = (NPCID_t)meetNPCs[r].quest.secondNPCID;
        bool isEventQuest = meetNPCs[r].eventQuest != 0;
        int questLevel = (int)meetNPCs[r].questLevel;

        MeetNPCQuestInfo* pMeetNPCQI =
            new MeetNPCQuestInfo(qID, race, MaxGrade, MinGrade, timeLimit, rewardClass, npcID, npcID2);
        pMeetNPCQI->setEventQuest(isEventQuest);
        pMeetNPCQI->setQuestLevel(questLevel);
        addQuestInfo(pMeetNPCQI);

        de::diagnosticTrace([&](std::ostream& output) { output << "Loading Quest Info : " << pMeetNPCQI->toString(); });
    }

    vector<EventMiniGameQuestRow> miniGames = defaultQuestInfoRepository().loadEventMiniGameQuestsOfNPC(name);

    for (size_t r = 0; r < miniGames.size(); r++) {
        QuestID_t qID = (QuestID_t)miniGames[r].quest.head.questID;
        Race_t race = (Race_t)miniGames[r].quest.head.race;
        QuestGrade_t MaxGrade = (QuestGrade_t)miniGames[r].quest.head.maxGrade;
        QuestGrade_t MinGrade = (QuestGrade_t)miniGames[r].quest.head.minGrade;
        DWORD timeLimit = (DWORD)miniGames[r].quest.head.timeLimitSec;
        RewardClass_t rewardClass = (RewardClass_t)miniGames[r].quest.head.rewardClass;
        int GameType = (int)miniGames[r].quest.gameType;
        bool isEventQuest = miniGames[r].eventQuest != 0;
        int questLevel = (int)miniGames[r].questLevel;

        MiniGameQuestInfo* pMiniGameQI =
            new MiniGameQuestInfo(qID, race, MaxGrade, MinGrade, timeLimit, rewardClass, GameType);
        pMiniGameQI->setEventQuest(isEventQuest);
        pMiniGameQI->setQuestLevel(questLevel);
        addQuestInfo(pMiniGameQI);

        de::diagnosticTrace(
            [&](std::ostream& output) { output << "Loading Quest Info : " << pMiniGameQI->toString(); });
    }

    __END_CATCH
}
