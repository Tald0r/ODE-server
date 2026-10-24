//////////////////////////////////////////////////////////////////////////////
// Filename    : DarkLightInfo.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "DarkLightInfo.h"

#include "Assert.h"
#include "GameContext.h"
#include "GameTime.h"
#include "TimeManager.h"
#include "Zone.h"
#include "repository/GameInfoRepository.h"

const int DLIndexByTimeband[4] = {
    // (month-1)*(24*6) + (hour)*6 + (minute/10)
    (5 - 1) * (24 * 6) + (5) * 6 + (50 / 10), // TIME_DAWM
    (5 - 1) * (24 * 6) + (12) * 6 + (0 / 10), // TIME_DAY
    (5 - 1) * (24 * 6) + (18) * 6 + (0 / 10), // TIME_DUSK
    (5 - 1) * (24 * 6) + (0) * 6 + (0 / 10)   // TIME_NIGHT
};

//////////////////////////////////////////////////////////////////////////////
// class DarkLightInfo member methods
//////////////////////////////////////////////////////////////////////////////

DarkLightInfo::DarkLightInfo()

{
    m_DarkLevel = 0;
    m_LightLevel = 0;
}

DarkLightInfo::DarkLightInfo(const DarkLightInfo& dli)

{
    m_DarkLevel = dli.m_DarkLevel;
    m_LightLevel = dli.m_LightLevel;
}

string DarkLightInfo::toString() const

{
    StringStream msg;
    msg << "DarkLightInfo(" << ",DarkLevel:" << (int)m_DarkLevel << ",LightLevel:" << (int)m_LightLevel << ")";
    return msg.toString();
}


//////////////////////////////////////////////////////////////////////////////
// class DarkLightInfoManager member methods
//////////////////////////////////////////////////////////////////////////////

DarkLightInfoManager::DarkLightInfoManager()

{
    for (uint i = 0; i < nDarkLightInfos; i++)
        m_DarkLightInfos[i] = NULL;
}

DarkLightInfoManager::~DarkLightInfoManager()

{
    for (uint i = 0; i < nDarkLightInfos; i++)
        SAFE_DELETE(m_DarkLightInfos[i]);
}

void DarkLightInfoManager::init()

{
    load();
}

void DarkLightInfoManager::load()

{
    vector<DarkLightRow> rows = defaultGameInfoRepository().loadDarkLight();

    for (size_t r = 0; r < rows.size(); r++) {
        BYTE month = rows[r].month;
        Assert(month != 0 && month <= 12);

        BYTE hour = rows[r].hour;
        Assert(hour < 24);

        BYTE minute = rows[r].minute;
        Assert(minute < 60 && minute % 10 == 0);

        DarkLightInfo* pDIInfo = new DarkLightInfo();
        pDIInfo->setDarkLevel(rows[r].darkLevel);
        pDIInfo->setLightLevel(rows[r].lightLevel);

        // Months run 1-12, hours 0-23, and minutes are 0, 10, 20, 30, 40, 50.
        int index = (month - 1) * (24 * 6) + (hour) * 6 + (minute / 10);
        Assert(m_DarkLightInfos[index] == NULL);
        m_DarkLightInfos[index] = pDIInfo;
    }

    // Unspecified slots are filled by copying the previous value.
    Assert(m_DarkLightInfos[0] != NULL);

    for (uint i = 1; i < nDarkLightInfos; i++) {
        if (m_DarkLightInfos[i] == NULL) {
            m_DarkLightInfos[i] = new DarkLightInfo(*m_DarkLightInfos[i - 1]);
        }
    }
}

DarkLightInfo* DarkLightInfoManager::getDarkLightInfo(BYTE month, BYTE hour, BYTE minute) {
    if (month == 0 || month > 12)
        throw OutOfBoundException("invalid month");

    if (hour > 23)
        throw OutOfBoundException("invalid hour");

    if (minute > 59)
        throw OutOfBoundException("invalid minute");

    return m_DarkLightInfos[(month - 1) * (24 * 6) + (hour) * 6 + (minute / 10)];
}

const DarkLightInfo* DarkLightInfoManager::getDarkLightInfo(BYTE month, BYTE hour, BYTE minute) const {
    if (month == 0 || month > 12)
        throw OutOfBoundException("invalid month");

    if (hour > 23)
        throw OutOfBoundException("invalid hour");

    if (minute > 59)
        throw OutOfBoundException("invalid minute");

    return m_DarkLightInfos[(month - 1) * (24 * 6) + (hour) * 6 + (minute / 10)];
}

const DarkLightInfo* DarkLightInfoManager::getCurrentDarkLightInfo(Zone* pZone) const

{
    // If time is stopped in the zone, return the matching DarkLight info.
    if (pZone != NULL && pZone->isTimeStop()) {
        return m_DarkLightInfos[DLIndexByTimeband[pZone->getTimeband()]];
    }

    // Get the game time from the global time manager.
    GameTime gametime = de::gameContext().worldTime().getGameTime();

    return getDarkLightInfo(gametime.getMonth(), gametime.getHour(), gametime.getMinute());
}

DarkLightInfo* DarkLightInfoManager::getCurrentDarkLightInfo(Zone* pZone)

{
    // If time is stopped in the zone, return the matching DarkLight info.
    if (pZone != NULL && pZone->isTimeStop()) {
        return m_DarkLightInfos[DLIndexByTimeband[pZone->getTimeband()]];
    }

    // Get the game time from the global time manager.
    GameTime gametime = de::gameContext().worldTime().getGameTime();

    return getDarkLightInfo(gametime.getMonth(), gametime.getHour(), gametime.getMinute());
}

string DarkLightInfoManager::toString() const

{
    StringStream msg;

    msg << "DarkLightInfoManager(";

    for (uint i = 0; i < nDarkLightInfos; i++) {
        int month = i / (24 * 6 + 6);
        int hour = (i % (24 * 6 + 6)) / 6;
        int minute = ((i % (24 * 6 + 6)) % 6) * 10;
        msg << month + 1 << "-" << hour << "-" << minute << " : " << (int)m_DarkLightInfos[i]->getDarkLevel() << "/"
            << (int)m_DarkLightInfos[i]->getLightLevel();
    }

    msg << ")";

    return msg.toString();
}
