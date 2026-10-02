#ifndef DARKEDEN_LOGIN_CHARACTER_TOPOLOGY_H
#define DARKEDEN_LOGIN_CHARACTER_TOPOLOGY_H

#include "CharacterSelection.h"
#include "GameServerInfoManager.h"
#include "ZoneGroupInfoManager.h"
#include "ZoneInfoManager.h"

// Borrows managers that outlive the view. Quiescent reloads are observed on the
// next lookup; missing catalogue references remain configuration exceptions.
class LoginCharacterTopology : public SelectPCTopology {
public:
    LoginCharacterTopology(const GameServerInfoManager& servers, const ZoneInfoManager& zones,
                           const ZoneGroupInfoManager& groups)
        : m_Servers(servers), m_Zones(zones), m_Groups(groups) {}

    bool isNonPKServer(WorldID_t worldID, ServerGroupID_t serverGroupID) override {
        return m_Servers.getGameServerInfo(1, serverGroupID, worldID)->isNonPKServer();
    }

    ServerID_t zoneServerID(ZoneID_t zoneID) override {
        const auto* zone = m_Zones.getZoneInfo(zoneID);
        return m_Groups.getZoneGroupInfo(zone->getZoneGroupID())->getServerID();
    }

private:
    const GameServerInfoManager& m_Servers;
    const ZoneInfoManager& m_Zones;
    const ZoneGroupInfoManager& m_Groups;
};

#endif
