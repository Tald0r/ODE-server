// The login server's zone-group-to-server routing catalogue.
#ifndef __ZONE_GROUP_INFO_MANAGER_H__
#define __ZONE_GROUP_INFO_MANAGER_H__

#include <unordered_map>

#include "ZoneGroupInfo.h"

class LoginConfigRepository;

// Load with quiescent readers. Failure preserves borrowed row pointers;
// successful replacement invalidates them and releases removed routes.
class ZoneGroupInfoManager {
public:
    ZoneGroupInfoManager() = default;
    ZoneGroupInfoManager(const ZoneGroupInfoManager&) = delete;
    ZoneGroupInfoManager& operator=(const ZoneGroupInfoManager&) = delete;

    void init();
    void load();
    void load(LoginConfigRepository& repository);

    const ZoneGroupInfo* getZoneGroupInfo(ZoneGroupID_t zoneGroupID) const;
    uint getSize() const noexcept {
        return m_ZoneGroupInfos.size();
    }
    string toString() const;

private:
    std::unordered_map<ZoneGroupID_t, ZoneGroupInfo> m_ZoneGroupInfos;
};

#endif
