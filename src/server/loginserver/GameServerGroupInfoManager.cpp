#include "GameServerGroupInfoManager.h"

#include <utility>

#include "DatabaseError.h"
#include "repository/LoginConfigRepository.h"

void GameServerGroupInfoManager::init() {
    load();
    cout << toString() << endl;
}

void GameServerGroupInfoManager::load() {
    load(defaultLoginConfigRepository());
}

void GameServerGroupInfoManager::load(LoginConfigRepository& repo) {
    int maxWorldID = 0;
    if (!repo.loadMaxGameServerGroupWorldID(maxWorldID))
        throw Error("GameServerGroupInfo TABLE does not exist!");
    if (!std::in_range<WorldID_t>(maxWorldID))
        throw Error("invalid login group maximum world ID");

    std::vector<LoginGameServerGroupRow> rows;
    try {
        rows = repo.loadGameServerGroups();
    } catch (const DatabaseError& error) {
        throw Error("GameServerGroupInfoManager::load : " + error.message());
    }

    // Keep the existing padding without narrowing the array dimension to a byte.
    decltype(m_Groups) replacement(static_cast<std::size_t>(maxWorldID) + 2);
    for (const auto& row : rows) {
        if (!std::in_range<WorldID_t>(row.worldID) || row.worldID > maxWorldID ||
            !std::in_range<ServerGroupID_t>(row.groupID) || !std::in_range<BYTE>(row.stat))
            throw Error("invalid login group world, group or status");

        const auto worldID = static_cast<WorldID_t>(row.worldID);
        const auto groupID = static_cast<ServerGroupID_t>(row.groupID);
        auto [entry, inserted] = replacement[worldID].try_emplace(groupID);
        if (!inserted)
            throw DuplicatedException("duplicated game-server nickname");
        auto& group = entry->second;
        group.setWorldID(worldID);
        group.setGroupID(groupID);
        group.setGroupName(row.groupName);
        group.setStat(static_cast<BYTE>(row.stat));
        cout << "addGameServerGroupInfo: " << (int)worldID << ", " << (int)groupID << " : "
             << group.getGroupName().c_str() << endl;
    }
    // All allocation, validation and load reporting finish before publication.
    m_Groups.swap(replacement);
}

const GameServerGroupInfo* GameServerGroupInfoManager::getGameServerGroupInfo(ServerGroupID_t groupID,
                                                                              WorldID_t worldID) const {
    if (worldID >= m_Groups.size())
        throw NoSuchElementException();
    const auto entry = m_Groups[worldID].find(groupID);
    if (entry == m_Groups[worldID].end())
        throw NoSuchElementException();
    return &entry->second;
}

uint GameServerGroupInfoManager::getSize(WorldID_t worldID) const noexcept {
    return worldID < m_Groups.size() ? m_Groups[worldID].size() : 0;
}

string GameServerGroupInfoManager::toString() const {
    StringStream msg;
    msg << "GameServerGroupInfoManager(\n";
    for (const auto& groups : m_Groups) {
        if (groups.empty())
            msg << "EMPTY";
        else
            for (const auto& [id, group] : groups)
                msg << group.toString() << '\n';
    }
    msg << ")";
    return msg.toString();
}
