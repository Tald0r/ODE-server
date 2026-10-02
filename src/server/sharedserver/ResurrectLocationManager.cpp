//////////////////////////////////////////////////////////////////////////////
// Filename    : ResurrectLocationManager.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ResurrectLocationManager.h"

#include <utility>

#include "repository/SharedConfigRepository.h"

void ResurrectLocationManager::init() {
    load();
}

void ResurrectLocationManager::load() {
    load(defaultSharedConfigRepository());
}

void ResurrectLocationManager::load(SharedConfigRepository& repository) {
    const auto rows = repository.loadResurrectLocations();
    if (rows.empty()) {
        cerr << "ResurrectLocationManager::load() : TABLE DOES NOT EXIST!" << endl;
        throw Error("ResurrectLocationManager::load() : TABLE DOES NOT EXIST!");
    }

    decltype(m_Positions) replacement;
    for (const auto& row : rows) {
        if (!std::in_range<ZoneID_t>(row.zoneID) || !std::in_range<ZoneID_t>(row.slayerZoneID) ||
            !std::in_range<ZoneCoord_t>(row.slayerX) || !std::in_range<ZoneCoord_t>(row.slayerY) ||
            !std::in_range<ZoneID_t>(row.vampireZoneID) || !std::in_range<ZoneCoord_t>(row.vampireX) ||
            !std::in_range<ZoneCoord_t>(row.vampireY))
            throw Error("invalid resurrection location ID or coordinate");

        const Positions positions{{static_cast<ZoneID_t>(row.slayerZoneID), static_cast<ZoneCoord_t>(row.slayerX),
                                   static_cast<ZoneCoord_t>(row.slayerY)},
                                  {static_cast<ZoneID_t>(row.vampireZoneID), static_cast<ZoneCoord_t>(row.vampireX),
                                   static_cast<ZoneCoord_t>(row.vampireY)}};
        if (!replacement.emplace(static_cast<ZoneID_t>(row.zoneID), positions).second) {
            cerr << "ResurrectLocationManager::addPosition() : ZoneID already exist!" << endl;
            throw NoSuchElementException("ResurrectLocationManager::addPosition() : ZoneID already exist!");
        }
    }
    m_Positions.swap(replacement);
}

bool ResurrectLocationManager::getSlayerPosition(ZoneID_t id, ZONE_COORD& zoneCoord) const {
    const auto position = m_Positions.find(id);
    if (position == m_Positions.end()) {
        cerr << "ResurrectLocationManager::getPosition() : No Such ZoneID" << endl;
        return false;
    }
    zoneCoord = position->second.slayer;
    return true;
}

bool ResurrectLocationManager::getVampirePosition(ZoneID_t id, ZONE_COORD& zoneCoord) const {
    const auto position = m_Positions.find(id);
    if (position == m_Positions.end()) {
        cerr << "ResurrectLocationManager::getPosition() : No Such ZoneID" << endl;
        return false;
    }
    zoneCoord = position->second.vampire;
    return true;
}
