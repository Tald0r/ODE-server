#include "EventZoneInfo.h"

#include "PCManager.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/ZoneInfoRepository.h"

EventZoneInfo::EventZoneInfo(WORD eventID, ZoneID_t zoneID) : m_EventID(eventID) {
    m_pZone = getZoneByZoneID(zoneID);
    Assert(m_pZone != NULL);
}

ZoneID_t EventZoneInfo::getZoneID() const {
    return m_pZone->getZoneID();
}

bool EventZoneInfo::canEnter() const {
    if (!isEventOn())
        return false;

    const PCManager* pPCManager = m_pZone->getPCManager();
    if (pPCManager->getSize() >= m_PCLimit)
        return false;

    return true;
}

EventZoneInfo* ZoneEventInfo::getEventZoneInfo(ZoneID_t zoneID) const {
    unordered_map<ZoneID_t, EventZoneInfo*>::const_iterator itr = m_EventZoneInfos.find(zoneID);

    if (itr == m_EventZoneInfos.end())
        return NULL;
    return itr->second;
}

void ZoneEventInfo::addEventZoneInfo(EventZoneInfo* pEventZoneInfo) {
    m_EventZoneInfos[pEventZoneInfo->getZoneID()] = pEventZoneInfo;
}

EventZoneInfo* ZoneEventInfo::getCurrentEventZoneInfo() const {
    unordered_map<ZoneID_t, EventZoneInfo*>::const_iterator itr = m_EventZoneInfos.begin();
    unordered_map<ZoneID_t, EventZoneInfo*>::const_iterator endItr = m_EventZoneInfos.end();

    for (; itr != endItr; ++itr) {
        if (itr->second->isEventOn())
            return itr->second;
    }

    return NULL;
}

ZoneEventInfo* EventZoneInfoManager::getZoneEventInfo(WORD eventID) const {
    unordered_map<WORD, ZoneEventInfo*>::const_iterator itr = m_ZoneEventInfos.find(eventID);

    if (itr == m_ZoneEventInfos.end())
        return NULL;
    return itr->second;
}

EventZoneInfo* EventZoneInfoManager::getEventZoneInfo(ZoneID_t zoneID) const {
    unordered_map<ZoneID_t, EventZoneInfo*>::const_iterator itr = m_EventZoneInfos.find(zoneID);

    if (itr == m_EventZoneInfos.end())
        return NULL;
    return itr->second;
}

void EventZoneInfoManager::load() {
    vector<EventZoneRow> rows = defaultZoneInfoRepository().loadEventZones();

    for (size_t r = 0; r < rows.size(); r++) {
        WORD eventID = rows[r].eventID;
        ZoneID_t zoneID = rows[r].zoneID;
        EventZoneInfo* pEventZoneInfo = new EventZoneInfo(eventID, zoneID);

        pEventZoneInfo->m_EnterX = rows[r].enterX;
        pEventZoneInfo->m_EnterY = rows[r].enterY;
        pEventZoneInfo->m_ResurrectX = rows[r].resurrectX;
        pEventZoneInfo->m_ResurrectY = rows[r].resurrectY;

        pEventZoneInfo->m_PCLimit = rows[r].pcLimit;
        pEventZoneInfo->m_bEventOn = false;

        if (m_ZoneEventInfos[eventID] == NULL) {
            m_ZoneEventInfos[eventID] = new ZoneEventInfo(eventID);
        }

        m_ZoneEventInfos[eventID]->addEventZoneInfo(pEventZoneInfo);
        m_EventZoneInfos[zoneID] = pEventZoneInfo;
    }
}
