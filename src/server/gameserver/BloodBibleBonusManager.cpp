//////////////////////////////////////////////////////////////////////////////
// Filename    : BloodBibleBonusManager.cpp
// Written By  : beowulf
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BloodBibleBonusManager.h"

#include "BloodBibleBonus.h"
#include "GCHolyLandBonusInfo.h"
#include "repository/GameInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class BloodBibleBonusManager member methods
//////////////////////////////////////////////////////////////////////////////

BloodBibleBonusManager::BloodBibleBonusManager()

{
    m_Count = 0;
}

BloodBibleBonusManager::~BloodBibleBonusManager()

{
    clear();
}

void BloodBibleBonusManager::init()

{
    load();
}

void BloodBibleBonusManager::clear()

{
    BloodBibleBonusHashMapItor itr = m_BloodBibleBonuses.begin();
    for (; itr != m_BloodBibleBonuses.end(); itr++) {
        SAFE_DELETE(itr->second);
    }

    m_BloodBibleBonuses.clear();
}

void BloodBibleBonusManager::load()

{
    __BEGIN_DEBUG

    clear();

    int maxType = 0;
    if (!defaultGameInfoRepository().loadMaxBloodBibleBonusType(maxType)) {
        throw Error("There is no data in BloodBibleBonusInfo Table");
    }

    m_Count = maxType + 1;

    Assert(m_Count > 0);

    vector<BloodBibleBonusRow> rows = defaultGameInfoRepository().loadBloodBibleBonuses();

    for (size_t r = 0; r < rows.size(); r++) {
        BloodBibleBonus* pBloodBibleBonus = new BloodBibleBonus();

        pBloodBibleBonus->setType(rows[r].type);
        pBloodBibleBonus->setName(rows[r].name);
        pBloodBibleBonus->setOptionTypeList(rows[r].optionList);
        pBloodBibleBonus->setRace(0);

        addBloodBibleBonus(pBloodBibleBonus);
    }

    __END_DEBUG
}

void BloodBibleBonusManager::save()

{
    throw UnsupportedError();
}

BloodBibleBonus* BloodBibleBonusManager::getBloodBibleBonus(BloodBibleBonusType_t bloodBibleBonusType) const {
    BloodBibleBonusHashMapConstItor itr = m_BloodBibleBonuses.find(bloodBibleBonusType);

    if (itr == m_BloodBibleBonuses.end()) {
        cerr << "BloodBibleBonusManager::getBloodBibleBonus() : no such element" << endl;
        throw NoSuchElementException();
    }

    return itr->second;
}

void BloodBibleBonusManager::addBloodBibleBonus(BloodBibleBonus* pBloodBibleBonus)

{
    Assert(pBloodBibleBonus != NULL);

    BloodBibleBonusHashMapConstItor itr = m_BloodBibleBonuses.find(pBloodBibleBonus->getType());
    if (itr != m_BloodBibleBonuses.end()) {
        throw DuplicatedException();
    }

    m_BloodBibleBonuses[pBloodBibleBonus->getType()] = pBloodBibleBonus;
}

void BloodBibleBonusManager::setBloodBibleBonusRace(BloodBibleBonusType_t bloodBibleBonusType, Race_t race)

{
    getBloodBibleBonus(bloodBibleBonusType)->setRace(race);
}

void BloodBibleBonusManager::makeHolyLandBonusInfo(GCHolyLandBonusInfo& gcHolyLandBonusInfo)

{}

string BloodBibleBonusManager::toString() const

{
    StringStream msg;

    msg << "BloodBibleBonusManager(\n";

    BloodBibleBonusHashMapConstItor itr = m_BloodBibleBonuses.begin();
    for (; itr != m_BloodBibleBonuses.end(); itr++) {
        msg << itr->second->toString() << ",";
    }

    return msg.toString();
}
