#include "PKZoneInfoManager.h"

#include "DiagnosticTrace.h"
#include "StringStream.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/ZoneInfoRepository.h"

string PKZoneInfo::toString()

{
    __BEGIN_TRY

    StringStream msg;

    msg << "PKZoneInfo(" << "ZoneID: " << getZoneID() << ",isFree: " << ((isFree()) ? ("true") : ("false"))
        << ",Race: " << getRace() << ",EnterPosition( " << getEnterX() << ", " << getEnterY() << ") "
        << ",PCLimit: " << getPCLimit() << ")";

    return msg.toString();

    __END_CATCH
}

void PKZoneInfoManager::load()

{
    __BEGIN_TRY

    vector<PKZoneRow> rows = defaultZoneInfoRepository().loadPKZones();

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneID_t zoneID = rows[r].zoneID;
        int race = rows[r].race;
        ZoneCoord_t X = rows[r].enterX;
        ZoneCoord_t Y = rows[r].enterY;
        ZoneCoord_t rX = rows[r].resurrectX;
        ZoneCoord_t rY = rows[r].resurrectY;
        int pcLimit = rows[r].pcLimit;

        PKZoneInfo* pPKZoneInfo = new PKZoneInfo(zoneID, X, Y, rX, rY, race, pcLimit);
        addPKZoneInfo(pPKZoneInfo);
    }

    __END_CATCH
}

void PKZoneInfoManager::addPKZoneInfo(PKZoneInfo* pPKZoneInfo)

{
    __BEGIN_TRY

    PKZoneInfoMap::iterator itr = m_PKZoneInfos.find(pPKZoneInfo->getZoneID());
    if (itr != m_PKZoneInfos.end())
        throw Error("Duplicated PK Zone ID");

    m_PKZoneInfos[pPKZoneInfo->getZoneID()] = pPKZoneInfo;

    Zone* pZone = getZoneByZoneID(pPKZoneInfo->getZoneID());
    if (pZone != NULL) {
        //		pZone->stopTime();
        //		pZone->setTimeband( 2 );
        //		pZone->resetDarkLightInfo();
    }

    de::diagnosticTrace([&](std::ostream& output) {
        output << "[" << pPKZoneInfo->getZoneID() << "]" << pPKZoneInfo->toString().c_str();
    });

    __END_CATCH
}

PKZoneInfo* PKZoneInfoManager::getPKZoneInfo(ZoneID_t zoneID) const

{
    __BEGIN_TRY

    PKZoneInfoMap::const_iterator itr = m_PKZoneInfos.find(zoneID);
    if (itr == m_PKZoneInfos.end())
        return NULL;

    return itr->second;

    __END_CATCH
}

bool PKZoneInfoManager::isPKZone(ZoneID_t zoneID) const

{
    __BEGIN_TRY

    PKZoneInfoMap::const_iterator itr = m_PKZoneInfos.find(zoneID);
    return itr != m_PKZoneInfos.end();

    __END_CATCH
}

bool PKZoneInfoManager::enterPKZone(ZoneID_t zoneID)

{
    __BEGIN_TRY

    PKZoneInfo* pPKZoneInfo = getPKZoneInfo(zoneID);
    if (pPKZoneInfo == NULL)
        return true;

    bool ret = false;

    __ENTER_CRITICAL_SECTION((*pPKZoneInfo))

    ret = pPKZoneInfo->enterZone();

    __LEAVE_CRITICAL_SECTION((*pPKZoneInfo))

    return ret;

    __END_CATCH
}

bool PKZoneInfoManager::leavePKZone(ZoneID_t zoneID)

{
    __BEGIN_TRY

    PKZoneInfo* pPKZoneInfo = getPKZoneInfo(zoneID);
    if (pPKZoneInfo == NULL)
        return true;

    bool ret = false;

    __ENTER_CRITICAL_SECTION((*pPKZoneInfo))

    ret = pPKZoneInfo->leaveZone();

    __LEAVE_CRITICAL_SECTION((*pPKZoneInfo))

    return ret;

    __END_CATCH
}

bool PKZoneInfoManager::canEnterPKZone(ZoneID_t zoneID)

{
    __BEGIN_TRY

    PKZoneInfo* pPKZoneInfo = getPKZoneInfo(zoneID);
    if (pPKZoneInfo == NULL)
        return true;

    bool ret = false;

    __ENTER_CRITICAL_SECTION((*pPKZoneInfo))

    ret = pPKZoneInfo->canEnter();

    __LEAVE_CRITICAL_SECTION((*pPKZoneInfo))

    return ret;

    __END_CATCH
}


bool PKZoneInfoManager::getResurrectPosition(ZoneID_t zoneID, ZONE_COORD& zoneCoord) const

{
    __BEGIN_TRY

    PKZoneInfo* pPKZoneInfo = getPKZoneInfo(zoneID);
    if (pPKZoneInfo == NULL)
        return false;

    zoneCoord.set(zoneID, pPKZoneInfo->getResurrectX(), pPKZoneInfo->getResurrectY());

    return true;

    __END_CATCH
}
