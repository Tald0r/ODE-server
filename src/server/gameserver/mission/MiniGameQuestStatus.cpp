#include "MiniGameQuestStatus.h"

#include "GCMiniGameScores.h"
#include "Ousters.h"
#include "PacketUtil.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "Slayer.h"
#include "StringStream.h"
#include "Vampire.h"

GCQuestStatus* MiniGameQuestStatus::makeStatusPacket() const {
    GCQuestStatus* pPacket = new GCQuestStatus();

    pPacket->setQuestID((WORD)getQuestID());
    pPacket->setCurrentNum((WORD)m_GameType);

    if (m_State == QUEST_REWARDED) {
        pPacket->setRemainTime(0);
    } else {
        pPacket->setRemainTime((DWORD)VSDateTime::currentDateTime().secsTo(m_Deadline));
    }

    return pPacket;
}

void MiniGameQuestStatus::executeWhenStart() {
    sendGCMiniGameScores(getOwnerPC(), m_GameType, 0);
}

void MiniGameQuestStatus::executeWhenFail() {
    PlayerCreature* pPC = getOwnerPC();
    Assert(pPC != NULL);

    // Failing it kills you.
    if (pPC->isSlayer()) {
        dynamic_cast<Slayer*>(pPC)->setHP(0);
    } else if (pPC->isVampire()) {
        dynamic_cast<Vampire*>(pPC)->setHP(0);
    } else if (pPC->isOusters()) {
        dynamic_cast<Ousters*>(pPC)->setHP(0);
    } else
        Assert(false);
}

string MiniGameQuestStatus::toStrong() const {
    StringStream msg;

    msg << "MiniGameQuestStatus(" << "Game Type : " << m_GameType
        << ", Score : " << ((m_bScoreSet) ? -1 : (int)m_GameScore) << ") : " << QuestStatus::toString().c_str();

    return msg.toString();
}
