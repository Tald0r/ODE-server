//////////////////////////////////////////////////////////////////////////////
// Filename    : SweeperBonusManager.cpp
// Written By  : beowulf
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SweeperBonusManager.h"

#include "GCSweeperBonusInfo.h"
#include "GameContext.h"
#include "LevelWarManager.h"
#include "LevelWarZoneInfoManager.h"
#include "SweeperBonus.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/WarInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class SweeperBonusManager member methods
//////////////////////////////////////////////////////////////////////////////

SweeperBonusManager::SweeperBonusManager()

{
    m_Count = 0;
}

SweeperBonusManager::~SweeperBonusManager()

{
    clear();
}

void SweeperBonusManager::init()

{
    load();
}

void SweeperBonusManager::clear()

{
    SweeperBonusHashMapItor itr = m_SweeperBonuses.begin();
    for (; itr != m_SweeperBonuses.end(); itr++) {
        SAFE_DELETE(itr->second);
    }

    m_SweeperBonuses.clear();
}

void SweeperBonusManager::load()

{
    __BEGIN_DEBUG

    clear();

    int maxType = 0;
    if (!defaultWarInfoRepository().loadMaxSweeperBonusType(maxType)) {
        throw Error("There is no data in SweeperBonusInfo Table");
    }

    m_Count = maxType + 1;

    Assert(m_Count > 0);

    vector<SweeperBonusRow> rows = defaultWarInfoRepository().loadSweeperBonuses();

    for (size_t r = 0; r < rows.size(); r++) {
        const SweeperBonusRow& row = rows[r];
        SweeperBonus* pSweeperBonus = new SweeperBonus();

        pSweeperBonus->setType(row.type);
        pSweeperBonus->setName(row.name);
        pSweeperBonus->setOptionTypeList(row.optionList);
        pSweeperBonus->setRace(row.ownerRace);
        pSweeperBonus->setLevel(row.level);

        addSweeperBonus(pSweeperBonus);
    }

    __END_DEBUG
}

void SweeperBonusManager::reloadOwner(int level)

{
    __BEGIN_DEBUG

    int maxType = 0;
    if (!defaultWarInfoRepository().loadMaxSweeperBonusType(maxType)) {
        throw Error("There is no data in SweeperBonusInfo Table");
    }

    m_Count = maxType + 1;

    Assert(m_Count > 0);

    vector<SweeperBonusOwnerRow> owners = defaultWarInfoRepository().loadSweeperBonusOwners(level);

    for (size_t r = 0; r < owners.size(); r++) {
        SweeperBonusType_t type = owners[r].type;

        SweeperBonusHashMapItor itr = m_SweeperBonuses.find(type);

        if (itr != m_SweeperBonuses.end()) {
            itr->second->setRace(owners[r].ownerRace);
        }
    }

    __END_DEBUG
}

void SweeperBonusManager::save()

{
    throw UnsupportedError();
}

SweeperBonus* SweeperBonusManager::getSweeperBonus(SweeperBonusType_t sweeperBonusType) const {
    SweeperBonusHashMapConstItor itr = m_SweeperBonuses.find(sweeperBonusType);

    if (itr == m_SweeperBonuses.end()) {
        cerr << "SweeperBonusManager::getSweeperBonus() : no such element" << endl;
        throw NoSuchElementException();
    }

    return itr->second;
}

void SweeperBonusManager::addSweeperBonus(SweeperBonus* pSweeperBonus)

{
    Assert(pSweeperBonus != NULL);

    SweeperBonusHashMapConstItor itr = m_SweeperBonuses.find(pSweeperBonus->getType());
    if (itr != m_SweeperBonuses.end()) {
        throw DuplicatedException();
    }

    m_SweeperBonuses[pSweeperBonus->getType()] = pSweeperBonus;
}


bool SweeperBonusManager::isAble(ZoneID_t zoneID) const {
    ZoneID_t levelWarZoneID;
    if (de::gameContext().levelWarZones().getLevelWarZoneID(zoneID, levelWarZoneID)) {
        Zone* pZone = getZoneByZoneID(levelWarZoneID);
        if (pZone == NULL)
            return false;

        LevelWarManager* pLevelWarManager = pZone->getLevelWarManager();
        if (pLevelWarManager == NULL)
            return false;

        return !pLevelWarManager->hasWar();
    }

    return false;
}

void SweeperBonusManager::setSweeperBonusRace(SweeperBonusType_t sweeperBonusType, Race_t race)

{
    getSweeperBonus(sweeperBonusType)->setRace(race);
}

void SweeperBonusManager::makeSweeperBonusInfo(GCSweeperBonusInfo& gcSweeperBonusInfo)

{
    SweeperBonusHashMapConstItor itr = m_SweeperBonuses.begin();
    for (; itr != m_SweeperBonuses.end(); itr++) {
        SweeperBonusInfo* pInfo = new SweeperBonusInfo();
        SweeperBonus* pBonus = itr->second;

        pInfo->setRace(pBonus->getRace());
        pInfo->setOptionType(pBonus->getOptionTypeList());

        gcSweeperBonusInfo.addSweeperBonusInfo(pInfo);
    }
}

void SweeperBonusManager::makeVoidSweeperBonusInfo(GCSweeperBonusInfo& gcSweeperBonusInfo)

{
    SweeperBonusHashMapConstItor itr = m_SweeperBonuses.begin();
    for (; itr != m_SweeperBonuses.end(); itr++) {
        SweeperBonusInfo* pInfo = new SweeperBonusInfo();
        SweeperBonus* pBonus = itr->second;

        pInfo->setRace(3);
        pInfo->setOptionType(pBonus->getOptionTypeList());

        gcSweeperBonusInfo.addSweeperBonusInfo(pInfo);
    }
}

string SweeperBonusManager::toString() const

{
    StringStream msg;

    msg << "SweeperBonusManager(\n";

    SweeperBonusHashMapConstItor itr = m_SweeperBonuses.begin();
    for (; itr != m_SweeperBonuses.end(); itr++) {
        msg << itr->second->toString() << ",";
    }

    return msg.toString();
}
