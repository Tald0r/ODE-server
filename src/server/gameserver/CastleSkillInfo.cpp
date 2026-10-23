//////////////////////////////////////////////////////////////////////////////
// Filename    : CastleSkillInfo.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CastleSkillInfo.h"

#include "Assert.h"
#include "Skill.h"
#include "repository/GameInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class CastleSkillInfo member methods
//////////////////////////////////////////////////////////////////////////////

CastleSkillInfo::CastleSkillInfo() {}

CastleSkillInfo::~CastleSkillInfo() {}

string CastleSkillInfo::toString() const {
    StringStream msg;
    msg << "CastleSkillInfo (" << "SkillType:" << (int)m_SkillType << ",ZoneID:" << (int)m_ZoneID << ")";
    return msg.toString();
}


//////////////////////////////////////////////////////////////////////////////
// class CastleSkillInfoManager member methods
//////////////////////////////////////////////////////////////////////////////

CastleSkillInfoManager::CastleSkillInfoManager()

{}

CastleSkillInfoManager::~CastleSkillInfoManager()

{
    try {
        clear();
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}

void CastleSkillInfoManager::clear()

{
    HashMapCastleSkillInfoItor itr = m_CastleSkillInfos.begin();

    for (; itr != m_CastleSkillInfos.end(); ++itr) {
        SAFE_DELETE(itr->second);
    }

    m_CastleSkillInfos.clear();
}

void CastleSkillInfoManager::load()

{
    __BEGIN_DEBUG

    vector<CastleSkillRow> rows = defaultGameInfoRepository().loadCastleSkills();

    for (size_t r = 0; r < rows.size(); r++) {
        CastleSkillInfo* pCastleSkillInfo = new CastleSkillInfo();

        pCastleSkillInfo->setSkillType(rows[r].skillType);
        pCastleSkillInfo->setZoneID(rows[r].zoneID);

        addCastleSkillInfo(pCastleSkillInfo);
    }

    __END_DEBUG
}

void CastleSkillInfoManager::addCastleSkillInfo(CastleSkillInfo* pCastleSkillInfo)

{
    Assert(pCastleSkillInfo != NULL);

    HashMapCastleSkillInfoItor itr = m_CastleSkillInfos.find(pCastleSkillInfo->getSkillType());

    if (itr != m_CastleSkillInfos.end()) {
        throw Error("CastleSkillInfoManager::addCastleSkillInfo : SkillType already exists.");
    }

    m_CastleSkillInfos[pCastleSkillInfo->getSkillType()] = pCastleSkillInfo;
}

SkillType_t CastleSkillInfoManager::getSkillType(ZoneID_t ZoneID) const {
    HashMapCastleSkillInfoConstItor itr = m_CastleSkillInfos.begin();

    for (; itr != m_CastleSkillInfos.end(); ++itr) {
        CastleSkillInfo* pCastleSkillInfo = itr->second;

        if (pCastleSkillInfo != NULL && pCastleSkillInfo->getZoneID() == ZoneID) {
            return pCastleSkillInfo->getSkillType();
        }
    }

    // None found.
    return SKILL_MAX;
}

ZoneID_t CastleSkillInfoManager::getZoneID(SkillType_t SkillType) const {
    HashMapCastleSkillInfoConstItor itr = m_CastleSkillInfos.find(SkillType);

    if (itr != m_CastleSkillInfos.end()) {
        return itr->second->getZoneID();
    }

    return 0;
}

string CastleSkillInfoManager::toString() const

{
    StringStream msg;

    msg << "CastleSkillInfoManager(";

    HashMapCastleSkillInfoConstItor itr = m_CastleSkillInfos.begin();

    for (; itr != m_CastleSkillInfos.end(); ++itr) {
        msg << itr->second->toString();
    }

    msg << ")";

    return msg.toString();
}
