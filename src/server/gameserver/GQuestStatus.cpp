#include "GQuestStatus.h"

#include "DiagnosticTrace.h"
#include "EffectEventQuestReset.h"
#include "GCGQuestStatusModify.h"
#include "GQuestInfo.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "repository/PlayRecordRepository.h"

// The missions belong to QuestStatusInfo, which frees them.
GQuestStatus::~GQuestStatus() {}

void GQuestStatus::initMissions() {
    map<vector<GQuestElement*>::const_iterator, GQuestMission*>::iterator itr = m_MissionMap.begin();
    for (; itr != m_MissionMap.end(); ++itr) {
        if (itr->second != NULL)
            (*(itr->second->m_Position))->whenMissionEnd(m_pOwner, itr->second);
    }

    m_MissionMap.clear();
    clearMissions();

    for (int i = GQuestInfo::HAPPEN; i < GQuestInfo::MAX; ++i) {
        m_ElementAdvance[i] = m_pGQuestInfo->getElements((GQuestInfo::ElementType)i).begin();
    }
}

void GQuestStatus::update() {
    GCGQuestStatusModify gcModify;
    gcModify.setType(checkMissions());
    gcModify.setInfo(this);
    m_pOwner->getPlayer()->sendPacket(&gcModify);
}

BYTE GQuestStatus::checkMissions() {
    BYTE ret = GCGQuestStatusModify::NO_MODIFY;
    de::diagnosticTrace([&](std::ostream& output) { output << "Checking quest : " << m_QuestID; });
    if (m_Status == DOING) {
        de::diagnosticTrace([&](std::ostream& output) { output << "Checking complete elements.."; });
        GQuestElement::ResultType resultComplete = checkElements(GQuestInfo::COMPLETE);
        switch (resultComplete) {
        case GQuestElement::OK: {
            de::diagnosticTrace([&](std::ostream& output) { output << "Quest success : " << m_QuestID; });
            m_Status = SUCCESS;
            ret = GCGQuestStatusModify::SUCCESS;
            cleanUpMissions();
            break;
        }
        case GQuestElement::FAIL: {
            de::diagnosticTrace([&](std::ostream& output) { output << "Quest failed : " << m_QuestID; });
            m_Status = FAIL;
            save();
            ret = GCGQuestStatusModify::FAIL;
            cleanUpMissions();
            break;
        }
        case GQuestElement::WAIT:
            de::diagnosticTrace([&](std::ostream& output) { output << "Quest waiting.. : " << m_QuestID; });
        default:
            break;
        }

        de::diagnosticTrace([&](std::ostream& output) { output << "Checking fail elements.."; });
        GQuestElement::ResultType resultFail = checkElements(GQuestInfo::FAIL);
        switch (resultFail) {
        case GQuestElement::OK: {
            de::diagnosticTrace([&](std::ostream& output) { output << "Quest failed : " << m_QuestID; });
            m_Status = FAIL;
            save();
            ret = GCGQuestStatusModify::FAIL;
            cleanUpMissions();
            break;
        }
        case GQuestElement::FAIL:
        case GQuestElement::WAIT:
            de::diagnosticTrace([&](std::ostream& output) { output << "Quest waiting.. : " << m_QuestID; });
        default:
            break;
        }
    }

    if (m_Status == SUCCESS) {
        de::diagnosticTrace([&](std::ostream& output) { output << "Checking reward elements.."; });
        GQuestElement::ResultType resultReward = checkElements(GQuestInfo::REWARD);
        switch (resultReward) {
        case GQuestElement::OK: {
            de::diagnosticTrace([&](std::ostream& output) { output << "Quest complete : " << m_QuestID; });
            m_Status = COMPLETE;
            save();
            m_pOwner->getGQuestManager()->refreshQuest();
            if (m_QuestID == 1001) {
                de::diagnosticTrace([&](std::ostream& output) { output << "complete.."; });
                EffectEventQuestReset* pEffect = new EffectEventQuestReset(m_pOwner, 1);
                int lastSec = 0;
                pEffect->setDeadline((EVENT_QUEST_TIME_LIMIT - lastSec) * 10);
                pEffect->setNextTime(((EVENT_QUEST_TIME_LIMIT - lastSec) % BROADCASTING_DELAY) * 10);
                m_pOwner->addEffect(pEffect);
            }
            break;
        }
        case GQuestElement::FAIL: {
            cout << "Quest failed : " << m_QuestID << endl;
            cout << "****** Quest Failed while Reward.. ******" << endl;
            m_Status = FAIL;
            save();
            break;
        }
        case GQuestElement::WAIT:
            de::diagnosticTrace([&](std::ostream& output) { output << "Quest waiting.. : " << m_QuestID; });
        default:
            break;
        }
    }

    if (m_Status == FAIL) {
        m_Status = CAN_REPLAY;
        save();
    }

    return ret;
}

GQuestElement::ResultType GQuestStatus::checkElements(GQuestInfo::ElementType type) {
    switch (m_pGQuestInfo->getCheckType(type)) {
    case GQuestInfo::SEQUENCE:
        return checkElementsSEQ(type);
    case GQuestInfo::AND:
        return checkElementsAND(type);
    case GQuestInfo::OR:
        return checkElementsOR(type);
    default:
        filelog("GQuestBug.log", "%u - GQuestStatus::checkElements(%u) : invalid checkType : %d",
                m_pGQuestInfo->getQuestID(), type, m_pGQuestInfo->getCheckType(type));
        Assert(false);
    }

    return GQuestElement::FAIL;
}

// Check the elements in order. The next one is checked only once the previous one completes.
GQuestElement::ResultType GQuestStatus::checkElementsSEQ(GQuestInfo::ElementType type) {
    de::diagnosticTrace([&](std::ostream& output) { output << "Checking SEQ : " << (int)type; });
    GQuestMission* pCurrentMission = m_MissionMap[m_ElementAdvance[type]];
    if (pCurrentMission != NULL) {
        de::diagnosticTrace(
            [&](std::ostream& output) { output << "Checking Mission : " << pCurrentMission->getMissionName(); });
        GQuestElement::ResultType result = (*m_ElementAdvance[type])->checkMission(pCurrentMission);
        de::diagnosticTrace([&](std::ostream& output) { output << "Result : " << result; });
        if (result == GQuestElement::FAIL)
            pCurrentMission->m_Status = MissionInfo::FAIL;
        if (result != GQuestElement::OK)
            return result;

        de::diagnosticTrace([&](std::ostream& output) { output << "Mission Complete"; });
        pCurrentMission->m_Status = MissionInfo::SUCCESS;
        (*m_ElementAdvance[type])->whenMissionEnd(m_pOwner, pCurrentMission);
        ++m_ElementAdvance[type];
    }

    for (; m_ElementAdvance[type] != m_pGQuestInfo->getElements(type).end(); ++m_ElementAdvance[type]) {
        de::diagnosticTrace([&](std::ostream& output) {
            output << "Checking Element : " << (*(m_ElementAdvance[type]))->getElementName();
        });
        GQuestElement::ResultType result = (*(m_ElementAdvance[type]))->checkCondition(m_pOwner);
        de::diagnosticTrace([&](std::ostream& output) { output << "Result : " << result; });
        if (result == GQuestElement::WAIT) {
            de::diagnosticTrace([&](std::ostream& output) { output << "Creating new mission..."; });
            GQuestMission* pNewMission = (*(m_ElementAdvance[type]))->makeInitMission(m_pOwner);
            if (pNewMission == NULL)
                return GQuestElement::FAIL;
            de::diagnosticTrace([&](std::ostream& output) { output << pNewMission->getMissionName() << " Created."; });

            pNewMission->m_Condition = type;
            pNewMission->m_Index = (*(m_ElementAdvance[type]))->getIndex();

            de::diagnosticTrace([&](std::ostream& output) { output << "Mission index is " << pNewMission->m_Index; });
            if (pNewMission->m_Index == 0)
                cout << "************ 0 index mission created!! ************" << endl;

            pNewMission->m_Status = MissionInfo::CURRENT;
            pNewMission->m_Position = m_ElementAdvance[type];
            pNewMission->m_pParent = this;
            m_Missions.push_back(pNewMission);
            m_MissionMap[m_ElementAdvance[type]] = pNewMission;
            (*(m_ElementAdvance[type]))->whenMissionStart(m_pOwner, pNewMission);
            // recursive call
            return checkElements(type);
        } else if (result == GQuestElement::FAIL) {
            return result;
        }
    }

    return GQuestElement::OK;
}

// OK if at least one succeeds. Used when checking FAIL conditions.
GQuestElement::ResultType GQuestStatus::checkElementsOR(GQuestInfo::ElementType type) {
    de::diagnosticTrace([&](std::ostream& output) { output << "Checking OR : " << (int)type; });

    vector<GQuestElement*>::const_iterator itr = m_pGQuestInfo->getElements(type).begin();
    vector<GQuestElement*>::const_iterator endItr = m_pGQuestInfo->getElements(type).end();

    for (; itr != endItr;) {
        GQuestMission* pMission = m_MissionMap[itr];
        if (pMission != NULL) {
            de::diagnosticTrace(
                [&](std::ostream& output) { output << "Checking Mission : " << pMission->getMissionName(); });
            if (pMission->m_Status == MissionInfo::SUCCESS) {
                de::diagnosticTrace([&](std::ostream& output) { output << "Mission already succeeded"; });
                return GQuestElement::OK;
            }

            if (pMission->m_Status == MissionInfo::FAIL) {
                de::diagnosticTrace([&](std::ostream& output) { output << "Mission already failed"; });
            } else {
                GQuestElement::ResultType result = (*itr)->checkMission(pMission);
                de::diagnosticTrace([&](std::ostream& output) { output << "Result : " << result; });
                if (result == GQuestElement::FAIL) {
                    de::diagnosticTrace([&](std::ostream& output) { output << "Mission Failed"; });
                    pMission->m_Status = MissionInfo::FAIL;
                    (*itr)->whenMissionEnd(m_pOwner, pMission);
                }

                if (result == GQuestElement::OK) {
                    de::diagnosticTrace([&](std::ostream& output) { output << "Mission Complete"; });
                    pMission->m_Status = MissionInfo::SUCCESS;
                    (*itr)->whenMissionEnd(m_pOwner, pMission);
                    return GQuestElement::OK;
                }
            }
        } else {
            de::diagnosticTrace(
                [&](std::ostream& output) { output << "Checking Element : " << (*itr)->getElementName(); });
            GQuestElement::ResultType result = (*itr)->checkCondition(m_pOwner);
            de::diagnosticTrace([&](std::ostream& output) { output << "Result : " << result; });
            if (result == GQuestElement::WAIT) {
                de::diagnosticTrace([&](std::ostream& output) { output << "Creating new mission..."; });
                GQuestMission* pNewMission = (*itr)->makeInitMission(m_pOwner);
                if (pNewMission == NULL) {
                    cout << "Mission creation failed" << endl;
                    ++itr;
                    continue;
                }

                de::diagnosticTrace(
                    [&](std::ostream& output) { output << pNewMission->getMissionName() << " Created."; });
                pNewMission->m_Condition = type;
                pNewMission->m_Index = (*itr)->getIndex();

                de::diagnosticTrace(
                    [&](std::ostream& output) { output << "Mission index is " << pNewMission->m_Index; });
                if (pNewMission->m_Index == 0)
                    cout << "************ 0 index mission created!! ************" << endl;

                pNewMission->m_Status = MissionInfo::CURRENT;
                pNewMission->m_Position = itr;
                pNewMission->m_pParent = this;
                m_Missions.push_back(pNewMission);
                m_MissionMap[itr] = pNewMission;
                (*itr)->whenMissionStart(m_pOwner, pNewMission);
                continue;
            } else if (result == GQuestElement::OK) {
                return result;
            }
        }

        ++itr;
    }

    return GQuestElement::FAIL;
}

// Evaluate every element at once. The ones that must wait are waited on together.
GQuestElement::ResultType GQuestStatus::checkElementsAND(GQuestInfo::ElementType type) {
    de::diagnosticTrace([&](std::ostream& output) { output << "Checking AND : " << (int)type; });

    vector<GQuestElement*>::const_iterator itr = m_pGQuestInfo->getElements(type).begin();
    vector<GQuestElement*>::const_iterator endItr = m_pGQuestInfo->getElements(type).end();

    GQuestElement::ResultType ret = GQuestElement::OK;

    for (; itr != endItr;) {
        GQuestMission* pMission = m_MissionMap[itr];
        if (pMission != NULL) {
            de::diagnosticTrace(
                [&](std::ostream& output) { output << "Checking Mission : " << pMission->getMissionName(); });
            if (pMission->m_Status == MissionInfo::FAIL) {
                de::diagnosticTrace([&](std::ostream& output) { output << "Mission already failed"; });
                return GQuestElement::FAIL;
            } else if (pMission->m_Status == MissionInfo::SUCCESS) {
                de::diagnosticTrace([&](std::ostream& output) { output << "Mission already succeeded"; });
            } else {
                GQuestElement::ResultType result = (*itr)->checkMission(pMission);
                de::diagnosticTrace([&](std::ostream& output) { output << "Result : " << result; });
                if (result == GQuestElement::OK) {
                    de::diagnosticTrace([&](std::ostream& output) { output << "Mission Success"; });
                    pMission->m_Status = MissionInfo::SUCCESS;
                    (*itr)->whenMissionEnd(m_pOwner, pMission);
                }

                if (result == GQuestElement::FAIL) {
                    de::diagnosticTrace([&](std::ostream& output) { output << "Mission Failed"; });
                    pMission->m_Status = MissionInfo::FAIL;
                    (*itr)->whenMissionEnd(m_pOwner, pMission);
                    return GQuestElement::OK;
                }
                if (result == GQuestElement::WAIT)
                    ret = GQuestElement::WAIT;
            }
        } else {
            de::diagnosticTrace(
                [&](std::ostream& output) { output << "Checking Element : " << (*itr)->getElementName(); });
            GQuestElement::ResultType result = (*itr)->checkCondition(m_pOwner);
            de::diagnosticTrace([&](std::ostream& output) { output << "Result : " << result; });
            if (result == GQuestElement::WAIT) {
                de::diagnosticTrace([&](std::ostream& output) { output << "Creating new mission..."; });
                GQuestMission* pNewMission = (*itr)->makeInitMission(m_pOwner);
                if (pNewMission == NULL) {
                    cout << "Mission creation failed" << endl;
                    ++itr;
                    continue;
                }

                de::diagnosticTrace(
                    [&](std::ostream& output) { output << pNewMission->getMissionName() << " Created."; });
                pNewMission->m_Condition = type;
                pNewMission->m_Index = (*itr)->getIndex();

                de::diagnosticTrace(
                    [&](std::ostream& output) { output << "Mission index is " << pNewMission->m_Index; });
                if (pNewMission->m_Index == 0)
                    cout << "************ 0 index mission created!! ************" << endl;

                pNewMission->m_Status = MissionInfo::CURRENT;
                pNewMission->m_Position = itr;
                pNewMission->m_pParent = this;
                m_Missions.push_back(pNewMission);
                m_MissionMap[itr] = pNewMission;
                (*itr)->whenMissionStart(m_pOwner, pNewMission);
                ret = GQuestElement::WAIT;
                continue;
            } else if (result == GQuestElement::FAIL) {
                return result;
            }
        }

        ++itr;
    }

    return ret;
}

void GQuestStatus::cleanUpMissions() {
    BYTE whatCond;
    if (m_Status == FAIL)
        whatCond = 1;
    else
        whatCond = 2;

    map<vector<GQuestElement*>::const_iterator, GQuestMission*>::iterator itr = m_MissionMap.begin();
    map<vector<GQuestElement*>::const_iterator, GQuestMission*>::iterator endItr = m_MissionMap.end();

    for (; itr != endItr; ++itr) {
        if (itr->second == NULL)
            continue;
        if (itr->second->m_Condition == whatCond) {
            itr->second->m_Status = MissionInfo::FAIL;
            (*(itr->second->m_Position))->whenMissionEnd(m_pOwner, itr->second);
        }
    }
}

void GQuestStatus::save() {
    __BEGIN_TRY

    defaultPlayRecordRepository().replaceSavedQuest(m_QuestID, m_pOwner->getName(), m_Status);

    __END_CATCH
}
