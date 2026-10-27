//////////////////////////////////////////////////////////////////////
//
// Filename    : ZoneTestAccess.h
// Description : Gives a Zone built in memory the managers its load()
//               would make from the database, so a runtime test can run
//               a zone's work without one. Zone befriends this class for
//               that purpose alone.
//
//////////////////////////////////////////////////////////////////////

#ifndef __ZONE_TEST_ACCESS_H__
#define __ZONE_TEST_ACCESS_H__

#include "Zone.h"

class ZoneTestAccess {
public:
    static void setLevelWarManager(Zone& zone, LevelWarManager* pManager) {
        zone.m_pLevelWarManager = pManager;
    }
};

#endif // __ZONE_TEST_ACCESS_H__
