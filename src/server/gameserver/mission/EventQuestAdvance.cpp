
#include "EventQuestAdvance.h"

#include <algorithm>

#include "DB.h"
#include "PlayerCreature.h"
#include "repository/QuestInfoRepository.h"

void EventQuestAdvance::save(const string& name) {
    if (!defaultQuestInfoRepository().updateEventQuestAdvance((uint)getStatus(), name, (uint)getLevel())) {
        defaultQuestInfoRepository().insertEventQuestAdvance((uint)getLevel(), name, (uint)getStatus());
    }
}

EventQuestAdvanceManager::EventQuestAdvanceManager(PlayerCreature* pPC) {
    m_Advances.reserve(EVENT_QUEST_LEVEL_MAX);
    m_Advances.clear();
    for (int i = 0; i < EVENT_QUEST_LEVEL_MAX; ++i)
        m_Advances[i] = NULL;

    m_pOwner = pPC;
}

template <typename T> inline void SafeDelete(T* pT) {
    SAFE_DELETE(pT);
}

EventQuestAdvanceManager::~EventQuestAdvanceManager() {
    clear();
}

void EventQuestAdvanceManager::clear() {
    for_each(m_Advances.begin(), m_Advances.end(), SafeDelete<EventQuestAdvance>);
    m_Advances.clear();
}

bool EventQuestAdvanceManager::start(int questLevel) {
    Assert(questLevel >= 0);
    Assert(questLevel < EVENT_QUEST_LEVEL_MAX);
    if (m_Advances[questLevel] == NULL)
        m_Advances[questLevel] = new EventQuestAdvance(questLevel);
    return m_Advances[questLevel]->start();
}

bool EventQuestAdvanceManager::success(int questLevel) {
    Assert(questLevel >= 0);
    Assert(questLevel < EVENT_QUEST_LEVEL_MAX);
    if (m_Advances[questLevel] != NULL)
        return m_Advances[questLevel]->success();
    return false;
}

bool EventQuestAdvanceManager::rewarded(int questLevel) {
    Assert(questLevel >= 0);
    Assert(questLevel < EVENT_QUEST_LEVEL_MAX);

    for (int i = 0; i < EVENT_QUEST_LEVEL_MAX; ++i) {
        if (m_Advances[i] != NULL)
            m_Advances[i]->reset();
    }
    return true;
}

bool EventQuestAdvanceManager::advanced(int questLevel) {
    Assert(questLevel >= 0);
    Assert(questLevel < EVENT_QUEST_LEVEL_MAX);
    if (m_Advances[questLevel] != NULL)
        return m_Advances[questLevel]->advanced();
    return false;
}

EventQuestAdvance::Status EventQuestAdvanceManager::getStatus(int questLevel) {
    Assert(questLevel < EVENT_QUEST_LEVEL_MAX);

    // A negative quest level means everyone has already finished the quest.
    if (questLevel < 0)
        return EventQuestAdvance::EVENT_QUEST_ADVANCED;
    if (m_Advances[questLevel] == NULL)
        return EventQuestAdvance::EVENT_QUEST_INIT;
    return m_Advances[questLevel]->getStatus();
}

void EventQuestAdvanceManager::save() {
    for (int i = 0; i < EVENT_QUEST_LEVEL_MAX; ++i) {
        if (m_Advances[i] != NULL)
            m_Advances[i]->save(m_pOwner->getName());
    }
}

void EventQuestAdvanceManager::save(int questLevel) {
    Assert(questLevel >= 0);
    Assert(questLevel < EVENT_QUEST_LEVEL_MAX);
    if (m_Advances[questLevel] != NULL)
        m_Advances[questLevel]->save(m_pOwner->getName());
}

void EventQuestAdvanceManager::load() {
    clear();

    vector<EventQuestAdvanceRow> rows = defaultQuestInfoRepository().loadEventQuestAdvances(m_pOwner->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        int qLevel = rows[r].questLevel;
        EventQuestAdvance::Status status = (EventQuestAdvance::Status)rows[r].status;

        m_Advances[qLevel] = new EventQuestAdvance(qLevel, status);
    }
}

bool EventQuestAdvanceManager::canExecute(int questLevel) {
    Assert(questLevel >= 0);
    Assert(questLevel < EVENT_QUEST_LEVEL_MAX);

    EventQuestAdvance::Status stat = getStatus(questLevel);

    if (stat == EventQuestAdvance::EVENT_QUEST_ADVANCED || stat == EventQuestAdvance::EVENT_QUEST_REWARDED)
        return false;

    for (int i = 0; i < questLevel; ++i) {
        if (getStatus(i) != EventQuestAdvance::EVENT_QUEST_ADVANCED)
            return false;
    }

    for (int i = questLevel + 1; i < EVENT_QUEST_LEVEL_MAX; ++i) {
        if (getStatus(i) != EventQuestAdvance::EVENT_QUEST_INIT)
            return false;
    }

    return true;
}

int EventQuestAdvanceManager::getQuestLevel() {
    for (int i = 0; i < EVENT_QUEST_LEVEL_MAX; ++i) {
        EventQuestAdvance::Status stat = getStatus(i);
        if (stat != EventQuestAdvance::EVENT_QUEST_ADVANCED) {
            if (stat == EventQuestAdvance::EVENT_QUEST_REWARDED)
                return 0;
            return i;
        }
    }

    return -1;
}
