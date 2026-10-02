// The login server's zone-to-group routing catalogue.
#ifndef __LOGIN_SERVER_ZONE_INFO_MANAGER_H__
#define __LOGIN_SERVER_ZONE_INFO_MANAGER_H__

#include <unordered_map>

#include "ZoneInfo.h"

class LoginConfigRepository;

// Load with quiescent readers. Failure preserves borrowed row pointers;
// successful replacement invalidates them and releases removed routes.
class ZoneInfoManager {
public:
    ZoneInfoManager() = default;
    ZoneInfoManager(const ZoneInfoManager&) = delete;
    ZoneInfoManager& operator=(const ZoneInfoManager&) = delete;

    void init();
    void load();
    void load(LoginConfigRepository& repository);

    const ZoneInfo* getZoneInfo(ZoneID_t zoneID) const;
    uint getSize() const noexcept {
        return m_ZoneInfos.size();
    }
    string toString() const;

private:
    std::unordered_map<ZoneID_t, ZoneInfo> m_ZoneInfos;
};

#endif
