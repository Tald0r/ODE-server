//////////////////////////////////////////////////////////////////////////////
// Filename    : ResurrectLocationManager.h
// Written by  : excel96
// Description : Resurrection locations for each zone.
//////////////////////////////////////////////////////////////////////////////

#ifndef __SHARED_SERVER_RESURRECT_LOCATION_MANAGER_H__
#define __SHARED_SERVER_RESURRECT_LOCATION_MANAGER_H__

#include <unordered_map>

#include "Exception.h"
#include "Types.h"

class SharedConfigRepository;

// Load with quiescent readers. Failed loads retain all previous positions;
// a successful load replaces both races together and removes absent zones.
class ResurrectLocationManager {
public:
    void init();
    void load();
    void load(SharedConfigRepository& repository);

    // A missing zone leaves the caller's coordinate unchanged.
    bool getSlayerPosition(ZoneID_t id, ZONE_COORD& zoneCoord) const;
    bool getVampirePosition(ZoneID_t id, ZONE_COORD& zoneCoord) const;

private:
    struct Positions {
        ZONE_COORD slayer;
        ZONE_COORD vampire;
    };
    std::unordered_map<ZoneID_t, Positions> m_Positions;
};

#endif
