// The login server's population counters, keyed by world and group.
#ifndef __USER_INFO_MANAGER_H__
#define __USER_INFO_MANAGER_H__

#include <vector>

#include <unordered_map>

#include "UserInfo.h"

class LoginConfigRepository;

// Load with quiescent users of borrowed counters. Failure preserves their
// identity and live counts; successful replacement starts the new counters at zero.
class UserInfoManager {
public:
    UserInfoManager() = default;
    UserInfoManager(const UserInfoManager&) = delete;
    UserInfoManager& operator=(const UserInfoManager&) = delete;

    void init();
    void load();
    void load(LoginConfigRepository& repository);

    UserInfo* getUserInfo(ZoneGroupID_t groupID, WorldID_t worldID);
    const UserInfo* getUserInfo(ZoneGroupID_t groupID, WorldID_t worldID) const;
    uint getSize(WorldID_t worldID) const noexcept;
    string toString() const;

private:
    using PopulationTable = std::unordered_map<ZoneGroupID_t, UserInfo>;
    std::vector<PopulationTable> m_Users;
};

#endif
