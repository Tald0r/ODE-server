#include "HolyLandManager.h"

#include "PCManager.h"
#include "PlayerCreature.h"
#include "ShrineInfoManager.h"
#include "WarZoneWork.h"
#include "Zone.h"

HolyLandManager::HolyLandManager()

{}

HolyLandManager::~HolyLandManager()

{
    __BEGIN_TRY

    clear();

    __END_CATCH_NO_RETHROW
}

void HolyLandManager::addHolyLand(Zone* pZone) {
    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapZoneItor itr = m_HolyLands.find(pZone->getZoneID());

    if (itr != m_HolyLands.end()) {
        throw DuplicatedException();
    }

    m_HolyLands[pZone->getZoneID()] = pZone;

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

void HolyLandManager::broadcast(Packet* pPacket) const {
    Assert(pPacket != NULL);

    de::war::postBroadcast(getHolyLandZoneIDs(), *pPacket);
}

vector<ZoneID_t> HolyLandManager::getHolyLandZoneIDs() const {
    vector<ZoneID_t> zoneIDs;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    for (const auto& holyLand : m_HolyLands)
        zoneIDs.push_back(holyLand.first);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return zoneIDs;
}

void HolyLandManager::fixTimeband(uint timeband) {
    de::war::postToZones(getHolyLandZoneIDs(), [timeband](Zone& zone) {
        zone.stopTime();
        zone.setTimeband(timeband);
        zone.resetDarkLightInfo();
    });
}

void HolyLandManager::resumeTimeband() {
    de::war::postToZones(getHolyLandZoneIDs(), [](Zone& zone) {
        zone.resumeTime();
        zone.resetDarkLightInfo();
    });
}

void HolyLandManager::killAllMonsters() {
    de::war::postToZones(getHolyLandZoneIDs(), [](Zone& zone) { zone.killAllMonsters(); });
}

void HolyLandManager::remainRaceWarPlayers() {
    de::war::postToZones(getHolyLandZoneIDs(), [](Zone& zone) { zone.remainRaceWarPlayers(); });
}

void HolyLandManager::refreshHolyLandPlayers()

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    HashMapZoneConstItor itr = m_HolyLands.begin();

    for (; itr != m_HolyLands.end(); itr++) {
        Zone* pZone = itr->second;
        Assert(pZone != NULL);

        pZone->setRefreshHolyLandPlayer(true);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}
