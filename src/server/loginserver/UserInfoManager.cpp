#include "UserInfoManager.h"

#include <utility>

#include "DatabaseError.h"
#include "repository/LoginConfigRepository.h"

void UserInfoManager::init() {
    load();
    cout << toString() << endl;
}

void UserInfoManager::load() {
    load(defaultLoginConfigRepository());
}

void UserInfoManager::load(LoginConfigRepository& repo) {
    int maxWorldID = 0;
    if (!repo.loadMaxGameServerGroupWorldID(maxWorldID))
        throw Error("GameServerGroupInfo TABLE does not exist!");
    if (!std::in_range<WorldID_t>(maxWorldID))
        throw Error("invalid login population maximum world ID");

    std::vector<LoginGameServerGroupIDRow> rows;
    try {
        rows = repo.loadGameServerGroupIDs();
    } catch (const DatabaseError& error) {
        throw Error("UserInfoManager::load : " + error.message());
    }

    decltype(m_Users) replacement(static_cast<std::size_t>(maxWorldID) + 2);
    for (const auto& row : rows) {
        if (!std::in_range<WorldID_t>(row.worldID) || row.worldID > maxWorldID ||
            !std::in_range<ZoneGroupID_t>(row.groupID))
            throw Error("invalid login population world or group");

        const auto worldID = static_cast<WorldID_t>(row.worldID);
        const auto groupID = static_cast<ZoneGroupID_t>(row.groupID);
        auto [entry, inserted] = replacement[worldID].try_emplace(groupID);
        if (!inserted)
            throw DuplicatedException("duplicated zone id");
        auto& user = entry->second;
        user.setWorldID(worldID);
        user.setServerGroupID(groupID);
        user.setUserNum(0);
    }
    m_Users.swap(replacement);
}

const UserInfo* UserInfoManager::getUserInfo(ZoneGroupID_t groupID, WorldID_t worldID) const {
    if (worldID < m_Users.size()) {
        const auto entry = m_Users[worldID].find(groupID);
        if (entry != m_Users[worldID].end())
            return &entry->second;
    }
    StringStream msg;
    msg << "ServerGroupID : " << groupID;
    throw NoSuchElementException(msg.toString());
}

UserInfo* UserInfoManager::getUserInfo(ZoneGroupID_t groupID, WorldID_t worldID) {
    return const_cast<UserInfo*>(std::as_const(*this).getUserInfo(groupID, worldID));
}

uint UserInfoManager::getSize(WorldID_t worldID) const noexcept {
    return worldID < m_Users.size() ? m_Users[worldID].size() : 0;
}

string UserInfoManager::toString() const {
    StringStream msg;
    msg << "UserInfoManager(";
    for (const auto& users : m_Users) {
        if (users.empty())
            msg << "EMPTY";
        else
            for (const auto& [id, user] : users)
                msg << user.toString();
    }
    msg << ")";
    return msg.toString();
}
