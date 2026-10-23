////////////////////////////////////////////////////////////////////////////////
// Filename    : VampEXPInfo.cpp
// Written By  : beowulf
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "VampEXPInfo.h"

#include "Assert.h"
#include "repository/BalanceInfoRepository.h"
// #include <algo.h>

////////////////////////////////////////////////////////////////////////////////
// class VampEXPInfo member methods
////////////////////////////////////////////////////////////////////////////////

VampEXPInfo::VampEXPInfo()

{}

VampEXPInfo::~VampEXPInfo()

    {__BEGIN_TRY __END_CATCH_NO_RETHROW}

string VampEXPInfo::toString() const

{
    StringStream msg;
    msg << "VampEXPInfo (" << " Level : " << (int)m_Level << " GoalExp : " << (int)m_GoalExp
        << " AccumExp : " << (int)m_AccumExp << ")";
    return msg.toString();
}

////////////////////////////////////////////////////////////////////////////////
// class VampEXPInfoManager member methods
////////////////////////////////////////////////////////////////////////////////

VampEXPInfoManager::VampEXPInfoManager()

{
    m_VampEXPCount = 0;
    m_VampEXPInfoList = NULL;
}

VampEXPInfoManager::~VampEXPInfoManager()

{
    __BEGIN_TRY

    if (m_VampEXPInfoList != NULL) {
        for (uint i = 0; i < m_VampEXPCount; i++)
            SAFE_DELETE(m_VampEXPInfoList[i]);

        SAFE_DELETE_ARRAY(m_VampEXPInfoList);
    }

    __END_CATCH_NO_RETHROW
}

void VampEXPInfoManager::init()

{
    load();
}

void VampEXPInfoManager::load()

{
    __BEGIN_DEBUG

    int maxLevel = 0;
    if (!defaultBalanceInfoRepository().loadMaxLevel(LEVEL_EXP_TABLE_VAMP_EXP, maxLevel)) {
        throw Error("There is no data in VampEXPInfo Table");
    }

    // Size the table from the highest level.
    m_VampEXPCount = maxLevel + 1;

    Assert(m_VampEXPCount > 0);

    m_VampEXPInfoList = new VampEXPInfo*[m_VampEXPCount];

    Assert(m_VampEXPInfoList != NULL);

    // Clear the array.
    for (uint i = 0; i < m_VampEXPCount; i++)
        m_VampEXPInfoList[i] = NULL;

    vector<LevelExpRow> rows = defaultBalanceInfoRepository().loadLevels(LEVEL_EXP_TABLE_VAMP_EXP);

    for (size_t r = 0; r < rows.size(); r++) {
        VampEXPInfo* pVampEXPInfo = new VampEXPInfo();

        pVampEXPInfo->setLevel(rows[r].level);
        pVampEXPInfo->setGoalExp(rows[r].goalExp);
        pVampEXPInfo->setAccumExp(rows[r].accumExp);

        addVampEXPInfo(pVampEXPInfo);
    }

    __END_DEBUG
}

void VampEXPInfoManager::addVampEXPInfo(VampEXPInfo* pVampEXPInfo)

{
    Assert(pVampEXPInfo != NULL);
    Assert(m_VampEXPInfoList[pVampEXPInfo->getLevel()] == NULL);

    m_VampEXPInfoList[pVampEXPInfo->getLevel()] = pVampEXPInfo;
}

VampEXPInfo* VampEXPInfoManager::getVampEXPInfo(uint VampEXPType) const {
    Assert(VampEXPType < m_VampEXPCount);
    Assert(m_VampEXPInfoList[VampEXPType] != NULL);

    return m_VampEXPInfoList[VampEXPType];
}

string VampEXPInfoManager::toString() const

{
    StringStream msg;

    msg << "VampEXPInfoManager(";

    for (uint i = 0; i < m_VampEXPCount; i++) {
        if (m_VampEXPInfoList[i] != NULL) {
            msg << m_VampEXPInfoList[i]->toString();
        } else {
            msg << "NULL";
        }
    }

    msg << ")";

    return msg.toString();
}
