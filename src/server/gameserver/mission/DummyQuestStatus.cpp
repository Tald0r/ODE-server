
#include "DummyQuestStatus.h"

#include "StringStream.h"

GCQuestStatus* DummyQuestStatus::makeStatusPacket() const {
    GCQuestStatus* pPacket = new GCQuestStatus();

    pPacket->setQuestID((WORD)getQuestID());
    pPacket->setCurrentNum((WORD)0);
    if (m_State == QUEST_REWARDED) {
        pPacket->setRemainTime(0);
    } else {
        pPacket->setRemainTime((DWORD)VSDateTime::currentDateTime().secsTo(m_Deadline));
    }


    return pPacket;
}

string DummyQuestStatus::toString() const {
    StringStream msg;

    msg << "DummyQuestStatus(" << ") : " << QuestStatus::toString().c_str();

    return msg.toString();
}
