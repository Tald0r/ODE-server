////////////////////////////////////////////////////////////////////////////////
// Filename    : OustersEXPInfo.cpp
// Written By  : beowulf
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "OustersEXPInfo.h"

#include "Assert.h"
#include "repository/BalanceInfoRepository.h"
// #include <algo.h>

////////////////////////////////////////////////////////////////////////////////
// class OustersEXPInfo member methods
////////////////////////////////////////////////////////////////////////////////

OustersEXPInfo::OustersEXPInfo()

{}

OustersEXPInfo::~OustersEXPInfo()

{}

string OustersEXPInfo::toString() const

{
    StringStream msg;
    msg << "OustersEXPInfo (" << " Level : " << (int)m_Level << " GoalExp : " << (int)m_GoalExp
        << " AccumExp : " << (int)m_AccumExp << " SkillPointBonus : " << (int)m_SkillPointBonus << ")";
    return msg.toString();
}

////////////////////////////////////////////////////////////////////////////////
// class OustersEXPInfoManager member methods
////////////////////////////////////////////////////////////////////////////////

OustersEXPInfoManager::OustersEXPInfoManager()

{
    m_OustersEXPCount = 0;
    m_OustersEXPInfoList = NULL;
}

OustersEXPInfoManager::~OustersEXPInfoManager()

{
    try {
        if (m_OustersEXPInfoList != NULL) {
            for (uint i = 0; i < m_OustersEXPCount; i++)
                SAFE_DELETE(m_OustersEXPInfoList[i]);

            SAFE_DELETE_ARRAY(m_OustersEXPInfoList);
        }
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}

void OustersEXPInfoManager::init()

{
    load();
}

void OustersEXPInfoManager::load()

{
    __BEGIN_DEBUG

    int maxLevel = 0;
    if (!defaultBalanceInfoRepository().loadMaxLevel(LEVEL_EXP_TABLE_OUSTERS_EXP, maxLevel)) {
        throw Error("There is no data in OustersEXPInfo Table");
    }

    // Size the table from the highest level.
    m_OustersEXPCount = maxLevel + 1;

    Assert(m_OustersEXPCount > 0);

    m_OustersEXPInfoList = new OustersEXPInfo*[m_OustersEXPCount];

    Assert(m_OustersEXPInfoList != NULL);

    // Clear the array.
    for (uint i = 0; i < m_OustersEXPCount; i++)
        m_OustersEXPInfoList[i] = NULL;

    vector<LevelExpRow> rows = defaultBalanceInfoRepository().loadLevels(LEVEL_EXP_TABLE_OUSTERS_EXP);

    for (size_t r = 0; r < rows.size(); r++) {
        OustersEXPInfo* pOustersEXPInfo = new OustersEXPInfo();

        pOustersEXPInfo->setLevel(rows[r].level);
        pOustersEXPInfo->setGoalExp(rows[r].goalExp);
        pOustersEXPInfo->setAccumExp(rows[r].accumExp);
        pOustersEXPInfo->setSkillPointBonus((SkillBonus_t)rows[r].skillPointBonus);

        addOustersEXPInfo(pOustersEXPInfo);
    }

    __END_DEBUG
}

void OustersEXPInfoManager::addOustersEXPInfo(OustersEXPInfo* pOustersEXPInfo)

{
    Assert(pOustersEXPInfo != NULL);
    Assert(m_OustersEXPInfoList[pOustersEXPInfo->getLevel()] == NULL);

    m_OustersEXPInfoList[pOustersEXPInfo->getLevel()] = pOustersEXPInfo;
}

OustersEXPInfo* OustersEXPInfoManager::getOustersEXPInfo(uint OustersEXPType) const {
    Assert(OustersEXPType < m_OustersEXPCount);
    Assert(m_OustersEXPInfoList[OustersEXPType] != NULL);

    return m_OustersEXPInfoList[OustersEXPType];
}

string OustersEXPInfoManager::toString() const

{
    StringStream msg;

    msg << "OustersEXPInfoManager(";

    for (uint i = 0; i < m_OustersEXPCount; i++) {
        if (m_OustersEXPInfoList[i] != NULL) {
            msg << m_OustersEXPInfoList[i]->toString();
        } else {
            msg << "NULL";
        }
    }

    msg << ")";

    return msg.toString();
}
