//////////////////////////////////////////////////////////////////////////////
// Filename    : WayPoint.cpp
// Written by  : excel96
// Description :
// Class module that manages the set of way points for the Slayer's personal portal
//////////////////////////////////////////////////////////////////////////////

#include "WayPoint.h"

#include "Assert.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class WayPoint member methods
//////////////////////////////////////////////////////////////////////////////

// constructor
WayPoint::WayPoint() {
    m_ZoneCoord.id = 0;
    m_ZoneCoord.x = 0;
    m_ZoneCoord.y = 0;
    m_Race = 0;
}

// destructor
WayPoint::~WayPoint() {}

// key generator
ulonglong WayPoint::getKey(void) const {
    return getKey(m_ZoneCoord.id, m_ZoneCoord.x, m_ZoneCoord.y);
}

// static key generator
ulonglong WayPoint::getKey(ZoneID_t id, ZoneCoord_t x, ZoneCoord_t y) {
    ulonglong ID = id;
    ulonglong X = x;
    ulonglong Y = y;

    ulonglong key = (ID << 32) | (X << 16) | Y;
    return key;
}


//////////////////////////////////////////////////////////////////////////////
// class WaypointManager member methods
//////////////////////////////////////////////////////////////////////////////

// constructor
WayPointManager::WayPointManager() {}

// destructor
WayPointManager::~WayPointManager() {
    WayPointMap::iterator itr = begin();
    for (; itr != end(); itr++) {
        WayPoint* pWayPoint = itr->second;
        SAFE_DELETE(pWayPoint);
    }

    clear();
}

// load waypoint data from database
void WayPointManager::load(void)

{
    vector<WayPointRow> rows = defaultZoneInfoRepository().loadAllWayPoints();

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneID_t ZID = rows[r].zoneID;
        ZoneCoord_t ZX = rows[r].x;
        ZoneCoord_t ZY = rows[r].y;
        Race_t Race = rows[r].race;

        WayPoint* pWayPoint = new WayPoint;
        Assert(pWayPoint != NULL);
        pWayPoint->setZoneCoord(ZID, ZX, ZY);
        pWayPoint->setRace(Race);

        addWayPoint(pWayPoint);

        // Ousters way points must block the tile.
        if (Race == RACE_OUSTERS) {
            Zone* pZone = getZoneByZoneID(ZID);
            Tile& rTile = pZone->getTile(ZX, ZY);

            rTile.setBlocked(Creature::MOVE_MODE_WALKING);
            rTile.setBlocked(Creature::MOVE_MODE_BURROWING);
        }
    }
}

// if there's right waypoint, return true, else return false
bool WayPointManager::isValidWayPoint(ZoneID_t id, ZoneCoord_t x, ZoneCoord_t y, Race_t race)

{
    ulonglong key = WayPoint::getKey(id, x, y);
    WayPointMap::const_iterator itr = find(key);

    if (itr != end() && itr->second->getRace() == race)
        return true;

    return false;
}

// add waypoint data into waypoint map
void WayPointManager::addWayPoint(WayPoint* pWayPoint)

{
    Assert(pWayPoint != NULL);

    ulonglong key = pWayPoint->getKey();

    WayPointMap::const_iterator itr = find(key);

    if (itr != end()) {
        cerr << "WayPointManager::addWayPoint() : Duplicated WayPoint" << endl;
        cerr << "A helicopter and a ground elemental horn probably share coordinates." << endl;
        throw Error("WayPointManager::addWayPoint() : Duplicated WayPoint");
    }

    insert(WayPointMap::value_type(key, pWayPoint));
}
