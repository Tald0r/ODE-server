//////////////////////////////////////////////////////////////////////////////
// Filename    : EventRefreshHolyLandPlayer.cpp
// Written by  : bezz
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventRefreshHolyLandPlayer.h"

#include "GameContext.h"
#include "HolyLandManager.h"
// #include "BloodBibleBonusManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"

// #include "GCHolyLandBonusInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class EventRefreshHolyLandPlayer member methods
//////////////////////////////////////////////////////////////////////////////

EventRefreshHolyLandPlayer::EventRefreshHolyLandPlayer(GamePlayer* pGamePlayer)

    : Event(pGamePlayer) {}

void EventRefreshHolyLandPlayer::activate()

{
    const unordered_map<ZoneGroupID_t, ZoneGroup*>& zoneGroups = de::gameContext().zoneGroups().getZoneGroups();

    unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr = zoneGroups.begin();

    for (; itr != zoneGroups.end(); ++itr) {
        const std::shared_ptr<const ZoneGroup::ZoneMap> zones = itr->second->getZones();
        unordered_map<ZoneID_t, Zone*>::const_iterator zItr = zones->begin();

        for (; zItr != zones->end(); ++zItr) {
            //   cout << zItr->second->getZoneID() << " zone update" << endl;
            zItr->second->setRefreshHolyLandPlayer(true);
        }
    }

    // Recompute the info of the players in Adam's holy land. (blood bible bonus)
    //	g_pHolyLandManager->refreshHolyLandPlayers();
}

string EventRefreshHolyLandPlayer::toString() const

{
    StringStream msg;
    msg << "EventRefreshHolyLandPlayer(" << ")";
    return msg.toString();
}
