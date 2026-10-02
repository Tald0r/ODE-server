//----------------------------------------------------------------------
// Filename    : GameServerGroupInfoManager.cpp
// Written By  : Reiot
// Description :
//----------------------------------------------------------------------

#include "GameServerGroupInfoManager.h"

#include <limits>

#include "DatabaseError.h"
#include "repository/SharedConfigRepository.h"

void GameServerGroupInfoManager::init() {
    load();
    cout << toString() << endl;
}

void GameServerGroupInfoManager::load() {
    load(defaultSharedConfigRepository());
}

void GameServerGroupInfoManager::load(SharedConfigRepository& repo) {
    int maxWorldID = 0;
    if (!repo.loadMaxGameServerGroupWorldID(maxWorldID))
        throw Error("GameServerGroupInfo TABLE does not exist!");
    if (maxWorldID < 0 || maxWorldID > std::numeric_limits<WorldID_t>::max())
        throw Error("invalid maximum game-server WorldID");

    // Retain the legacy spare world slot without narrowing the array size.
    std::vector<Groups> replacement(static_cast<std::size_t>(maxWorldID) + 2);
    std::vector<SharedGameServerGroupRow> rows;
    try {
        rows = repo.loadGameServerGroups();
    } catch (const DatabaseError& error) {
        throw Error("GameServerGroupInfoManager::load : " + error.message());
    }
    for (const auto& row : rows) {
        if (row.worldID < 0 || row.worldID > maxWorldID || row.groupID < 0 ||
            row.groupID > std::numeric_limits<ServerGroupID_t>::max())
            throw Error("invalid game-server group catalogue ID");

        auto& world = replacement[static_cast<std::size_t>(row.worldID)];
        auto [entry, inserted] = world.try_emplace(static_cast<ServerGroupID_t>(row.groupID));
        if (!inserted)
            throw DuplicatedException("duplicated game-server nickname");
        auto& info = entry->second;
        info.setWorldID(static_cast<WorldID_t>(row.worldID));
        info.setGroupID(static_cast<ServerGroupID_t>(row.groupID));
        info.setGroupName(row.groupName);
    }
    // Includes world zero and every partially prepared row on any failure.
    m_Groups.swap(replacement);
}

const GameServerGroupInfo* GameServerGroupInfoManager::getGameServerGroupInfo(ServerGroupID_t groupID,
                                                                              WorldID_t worldID) const {
    if (worldID >= m_Groups.size())
        throw NoSuchElementException();
    const auto& world = m_Groups[worldID];
    const auto entry = world.find(groupID);
    if (entry == world.end())
        throw NoSuchElementException();
    return &entry->second;
}

uint GameServerGroupInfoManager::getSize(WorldID_t worldID) const {
    if (worldID >= m_Groups.size())
        throw NoSuchElementException();
    return m_Groups[worldID].size();
}

string GameServerGroupInfoManager::toString() const {
    StringStream msg;
    msg << "GameServerGroupInfoManager(\n";
    for (std::size_t world = 1; world < m_Groups.size(); ++world) {
        if (m_Groups[world].empty())
            msg << "EMPTY";
        else
            for (const auto& [id, info] : m_Groups[world])
                msg << info.toString() << '\n';
    }
    msg << ")";
    return msg.toString();
}
