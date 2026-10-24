//////////////////////////////////////////////////////////////////////////////
// Filename    : EventShutdown.cpp
// Written by  : bezz
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventShutdown.h"

#include "GameContext.h"
#include "IncomingPlayerManager.h"
#include "VariableManager.h"
#include "ZoneGroupManager.h"
#include "ZonePlayerManager.h"
#include "repository/ItemObjectRepository.h"
#include "repository/SystemAvailabilityRepository.h"
#include "signal.h"
//////////////////////////////////////////////////////////////////////////////
// class EventShutdown member methods
//////////////////////////////////////////////////////////////////////////////

EventShutdown::EventShutdown(GamePlayer* pGamePlayer)

    : Event(pGamePlayer) {}

void EventShutdown::activate()

{
    try {
        const unordered_map<ZoneGroupID_t, ZoneGroup*>& zoneGroups = de::gameContext().zoneGroups().getZoneGroups();
        unordered_map<ZoneGroupID_t, ZoneGroup*>::const_iterator itr = zoneGroups.begin();

        for (; itr != zoneGroups.end(); itr++) {
            ZonePlayerManager* pZonePlayerManager = itr->second->getZonePlayerManager();

            pZonePlayerManager->clearPlayers();
        }

        de::gameContext().incomingPlayers().clearPlayers();
    } catch (Throwable& t) {
        filelog("errorLog.txt", "EventShutdown could not clear the players: %s", t.toString().c_str());
    }

    // Terminate the process.
    if (de::gameContext().variables().isKillDaemonCtl() == 1) {
        kill(getppid(), 9);
    }

    //	kill( getppid(), 9 );

    kill(getpid(), 9);
}

string EventShutdown::toString() const

{
    StringStream msg;
    msg << "EventShutdown(" << ")";
    return msg.toString();
}
