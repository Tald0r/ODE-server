#include "LevelWarZoneInfoManager.h"

#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "WarZoneWork.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/ZoneInfoRepository.h"

LevelWarZoneInfo::LevelWarZoneInfo() {}

LevelWarZoneInfo::~LevelWarZoneInfo() {}

void LevelWarZoneInfo::setZoneIDList(const string& zoneIDs)

{
    makeZoneIDList(zoneIDs, m_LevelWarBonusZoneIDList);
}

bool LevelWarZoneInfo::isBonusZone(ZoneID_t targetZoneID) const

{
    list<ZoneID_t>::const_iterator itr = m_LevelWarBonusZoneIDList.begin();
    list<ZoneID_t>::const_iterator endItr = m_LevelWarBonusZoneIDList.end();

    for (; itr != endItr; itr++) {
        ZoneID_t zoneID = *itr;

        if (zoneID == targetZoneID)
            return true;
    }

    return false;
}

bool LevelWarZoneInfo::isCreatureThisLevel(Creature* pCreature) const

{
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        return (pSlayer->getTotalAttr(ATTR_BASIC) >= getMinSlayerSum() &&
                pSlayer->getTotalAttr(ATTR_BASIC) <= getMaxSlayerSum());
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        return (pVampire->getLevel() >= getMinVampireLevel() && pVampire->getLevel() <= getMaxVampireLevel());
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        return (pOusters->getLevel() >= getMinOustersLevel() && pOusters->getLevel() <= getMaxOustersLevel());
    }

    return false;
}

bool LevelWarZoneInfo::isZoneThisLevel(ZoneID_t zoneID) const

{
    list<ZoneID_t>::const_iterator itr = m_LevelWarBonusZoneIDList.begin();
    list<ZoneID_t>::const_iterator endItr = m_LevelWarBonusZoneIDList.end();

    for (; itr != endItr; itr++) {
        if (*itr == zoneID)
            return true;
    }

    return false;
}

string LevelWarZoneInfo::toString() const

{
    StringStream msg;
    msg << "LevelWarZoneInfo(" << "Grade : " << m_LevelWarGrade << ", ZoneID : " << m_ZoneID
        << ", SweeperTypeMin : " << m_SweeperTypeMin << ", SweeperTypeMax : " << m_SweeperTypeMax
        << ", SlayerSumMin : " << m_SlayerSumMin << ", SlayerSumMax : " << m_SlayerSumMax
        << ", VampireLevelMin : " << m_VampireLevelMin << ", VampireLevelMax : " << m_VampireLevelMax
        << ", OustersLevelMin : " << m_OustersLevelMin << ", OustersLevelMax : " << m_OustersLevelMax << ")";

    return msg.toString();
}

LevelWarZoneInfoManager::LevelWarZoneInfoManager() {}

LevelWarZoneInfoManager::~LevelWarZoneInfoManager() {
    __BEGIN_TRY

    unordered_map<ZoneID_t, LevelWarZoneInfo*>::iterator itr = m_LevelWarZoneInfos.begin();
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::iterator endItr = m_LevelWarZoneInfos.end();
    for (; itr != endItr; itr++) {
        LevelWarZoneInfo* pLevelWarZoneInfo = itr->second;
        SAFE_DELETE(pLevelWarZoneInfo);
    }

    m_LevelWarZoneInfos.clear();

    __END_CATCH_NO_RETHROW
}

void LevelWarZoneInfoManager::init()

{
    load();
}

void LevelWarZoneInfoManager::load()

{
    m_LevelWarZoneInfos.clear();
    clearLevelWarZoneIDs();

    vector<LevelWarZoneRow> rows = defaultZoneInfoRepository().loadLevelWarZones();

    for (size_t r = 0; r < rows.size(); r++) {
        LevelWarZoneInfo* pLevelWarZoneInfo = new LevelWarZoneInfo();

        pLevelWarZoneInfo->setGrade(rows[r].id);
        pLevelWarZoneInfo->setZoneID(rows[r].zoneID);

        pLevelWarZoneInfo->setMinSweeperBonusType(rows[r].sweeperTypeMin);
        pLevelWarZoneInfo->setMaxSweeperBonusType(rows[r].sweeperTypeMax);

        pLevelWarZoneInfo->setMinSlayerSum(rows[r].slayerMin);
        pLevelWarZoneInfo->setMaxSlayerSum(rows[r].slayerMax);

        pLevelWarZoneInfo->setMinVampireLevel(rows[r].vampireMin);
        pLevelWarZoneInfo->setMaxVampireLevel(rows[r].vampireMax);

        pLevelWarZoneInfo->setMinOustersLevel(rows[r].oustersMin);
        pLevelWarZoneInfo->setMaxOustersLevel(rows[r].oustersMax);

        pLevelWarZoneInfo->setZoneIDList(rows[r].zoneIDList);

        addLevelWarZoneInfo(pLevelWarZoneInfo);
    }
}

void LevelWarZoneInfoManager::addLevelWarZoneInfo(LevelWarZoneInfo* pLevelWarZoneInfo)

{
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::iterator itr = m_LevelWarZoneInfos.find(pLevelWarZoneInfo->getZoneID());

    if (itr != m_LevelWarZoneInfos.end())
        throw Error("duplicated zone id");

    m_LevelWarZoneInfos[pLevelWarZoneInfo->getZoneID()] = pLevelWarZoneInfo;

    const list<ZoneID_t>& zoneIDs = pLevelWarZoneInfo->getZoneIDList();

    list<ZoneID_t>::const_iterator zitr = zoneIDs.begin();
    list<ZoneID_t>::const_iterator zendItr = zoneIDs.end();

    for (; zitr != zendItr; zitr++) {
        ZoneID_t zoneID = *zitr;
        setLevelWarZoneID(zoneID, pLevelWarZoneInfo->getZoneID());
    }
}

int LevelWarZoneInfoManager::getCreatureLevelGrade(Creature* pCreature)

{
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::iterator itr = m_LevelWarZoneInfos.begin();
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::iterator endItr = m_LevelWarZoneInfos.end();

    for (; itr != endItr; itr++) {
        if (itr->second->isCreatureThisLevel(pCreature))
            return itr->second->getGrade();
    }

    return -1;
}

ZoneID_t LevelWarZoneInfoManager::getCreatureZoneID(Creature* pCreature)

{
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::iterator itr = m_LevelWarZoneInfos.begin();
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::iterator endItr = m_LevelWarZoneInfos.end();

    for (; itr != endItr; itr++) {
        if (itr->second->isCreatureThisLevel(pCreature))
            return itr->second->getZoneID();
    }

    return 1;
}

bool LevelWarZoneInfoManager::isCreatureBonusZone(Creature* pCreature, ZoneID_t zoneID) const

{
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::const_iterator itr = m_LevelWarZoneInfos.begin();
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::const_iterator endItr = m_LevelWarZoneInfos.end();

    for (; itr != endItr; itr++) {
        LevelWarZoneInfo* pLevelWarZoneInfo = itr->second;

        if (pLevelWarZoneInfo->isCreatureThisLevel(pCreature) && pLevelWarZoneInfo->isZoneThisLevel(zoneID)) {
            return true;
        }
    }

    return false;
}

LevelWarZoneInfo* LevelWarZoneInfoManager::getLevelWarZoneInfo(ZoneID_t zoneID) const

{
    unordered_map<ZoneID_t, LevelWarZoneInfo*>::const_iterator itr = m_LevelWarZoneInfos.find(zoneID);

    if (itr != m_LevelWarZoneInfos.end())
        return NULL;
    else
        return itr->second;
}

void LevelWarZoneInfoManager::broadcast(ZoneID_t zoneID, Packet* pPacket) const {
    Assert(pPacket != NULL);

    vector<ZoneID_t> zoneIDs;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<ZoneID_t, LevelWarZoneInfo*>::const_iterator itr = m_LevelWarZoneInfos.find(zoneID);

    if (itr != m_LevelWarZoneInfos.end()) {
        const list<ZoneID_t>& bonusZoneIDs = itr->second->getZoneIDList();
        zoneIDs.assign(bonusZoneIDs.begin(), bonusZoneIDs.end());
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // The bonus zones may belong to other groups than the level war's zone,
    // and the send marks each player it reaches, so it runs on each zone's
    // own thread.
    de::war::postBroadcast(zoneIDs, *pPacket,
                           [](Zone& zone, Packet& packet) { zone.broadcastLevelWarBonusPacket(&packet); });
}

void LevelWarZoneInfoManager::clearLevelWarZoneIDs()

{
    m_LevelWarZoneIDs.clear();
}

bool LevelWarZoneInfoManager::getLevelWarZoneID(ZoneID_t zoneID, ZoneID_t& levelWarZoneID) const

{
    unordered_map<ZoneID_t, ZoneID_t>::const_iterator itr = m_LevelWarZoneIDs.find(zoneID);

    if (itr != m_LevelWarZoneIDs.end()) {
        levelWarZoneID = itr->second;
        return true;
    }

    return false;
}
void LevelWarZoneInfoManager::setLevelWarZoneID(ZoneID_t zoneID, ZoneID_t levelWarZoneID)

{
    m_LevelWarZoneIDs[zoneID] = levelWarZoneID;
}

string LevelWarZoneInfoManager::toString() const

{
    StringStream msg;

    msg << "LevelWarZoneInfoManager(";

    if (m_LevelWarZoneInfos.empty())
        msg << "EMPTY";
    else {
        unordered_map<ZoneID_t, LevelWarZoneInfo*>::const_iterator itr = m_LevelWarZoneInfos.begin();
        unordered_map<ZoneID_t, LevelWarZoneInfo*>::const_iterator endItr = m_LevelWarZoneInfos.end();
        for (; itr != endItr; itr++) {
            msg << itr->second->toString();
        }
    }

    msg << ")";

    return msg.toString();
}
