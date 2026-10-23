//////////////////////////////////////////////////////////////////////////////
// Filename    : RankBonusInfo.cpp
// Written By  : beowulf
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "RankBonusInfo.h"

#include "Assert.h"
#include "repository/GameInfoRepository.h"
// #include <algo.h>

//////////////////////////////////////////////////////////////////////////////
// class RankBonusInfo member methods
//////////////////////////////////////////////////////////////////////////////

RankBonusInfo::RankBonusInfo() {}

RankBonusInfo::~RankBonusInfo(){__BEGIN_TRY __END_CATCH_NO_RETHROW}

string RankBonusInfo::toString() const {
    StringStream msg;
    msg << "RankBonusInfo (" << "Type:" << (int)m_Type << ",Name:" << m_Name << ",Rank:" << m_Rank
        << ",Point:" << m_Point << ")";
    return msg.toString();
}


//////////////////////////////////////////////////////////////////////////////
// class RankBonusInfoManager member methods
//////////////////////////////////////////////////////////////////////////////

RankBonusInfoManager::RankBonusInfoManager()

{
    m_Count = 0;
    m_RankBonusInfoList = NULL;
}

RankBonusInfoManager::~RankBonusInfoManager()

{
    __BEGIN_TRY

    SAFE_DELETE_ARRAY(m_RankBonusInfoList);

    __END_CATCH_NO_RETHROW
}

void RankBonusInfoManager::init()

{
    load();
}

void RankBonusInfoManager::clear()

{
    if (m_RankBonusInfoList != NULL) {
        for (uint i = 0; i < m_Count; i++) {
            if (m_RankBonusInfoList[i] != NULL)
                SAFE_DELETE(m_RankBonusInfoList[i]);
        }
    }
    SAFE_DELETE_ARRAY(m_RankBonusInfoList);
}

void RankBonusInfoManager::load()

{
    __BEGIN_DEBUG

    clear();

    int maxType = 0;
    if (!defaultGameInfoRepository().loadMaxRankBonusType(maxType)) {
        throw Error("There is no data in RankBonusInfo Table");
    }

    m_Count = maxType + 1;

    Assert(m_Count > 0);

    m_RankBonusInfoList = new RankBonusInfo*[m_Count];

    for (uint i = 0; i < m_Count; i++)
        m_RankBonusInfoList[i] = NULL;

    vector<RankBonusInfoRow> rows = defaultGameInfoRepository().loadRankBonusInfos();

    for (size_t r = 0; r < rows.size(); r++) {
        RankBonusInfo* pRankBonusInfo = new RankBonusInfo();

        pRankBonusInfo->setType(rows[r].type);
        pRankBonusInfo->setName(rows[r].name);
        pRankBonusInfo->setRank(rows[r].rank);
        pRankBonusInfo->setPoint(rows[r].point);
        pRankBonusInfo->setRace(rows[r].race);

        addRankBonusInfo(pRankBonusInfo);
    }

    __END_DEBUG
}

void RankBonusInfoManager::save()

{
    throw UnsupportedError();
}

RankBonusInfo* RankBonusInfoManager::getRankBonusInfo(DWORD rankBonusType) const {
    if (rankBonusType >= m_Count) {
        cerr << "RankBonusInfoManager::getRankBonusInfo() : out of bound" << endl;
        throw OutOfBoundException();
    }

    if (m_RankBonusInfoList[rankBonusType] == NULL) {
        cerr << "RankBonusInfoManager::getRankBonusInfo() : no such element" << endl;
        throw NoSuchElementException();
    }

    return m_RankBonusInfoList[rankBonusType];
}

void RankBonusInfoManager::addRankBonusInfo(RankBonusInfo* pRankBonusInfo)

{
    Assert(pRankBonusInfo != NULL);

    if (m_RankBonusInfoList[pRankBonusInfo->getType()] != NULL)
        throw DuplicatedException();

    m_RankBonusInfoList[pRankBonusInfo->getType()] = pRankBonusInfo;
}

string RankBonusInfoManager::toString() const

{
    StringStream msg;

    msg << "RankBonusInfoManager(\n";

    for (int i = 0; i < (int)m_Count; i++) {
        msg << "RankBonusInfos[" << i << "] == ";
        msg << (m_RankBonusInfoList[i] == NULL ? "NULL" : m_RankBonusInfoList[i]->getName());
        msg << "\n";
        if (m_RankBonusInfoList[i] != NULL) {
            msg << m_RankBonusInfoList[i]->toString() << "\n";
        }
    }

    return msg.toString();
}
