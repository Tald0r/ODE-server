//----------------------------------------------------------------------
// Filename    : GameWorldInfoManager.cpp
// Written By  : Reiot
// Description :
//----------------------------------------------------------------------

#include "GameWorldInfoManager.h"

#include <utility>

#include "DatabaseError.h"
#include "repository/ServerInfoRepository.h"

void GameWorldInfoManager::init() {
    load();
    cout << toString() << endl;
}

void GameWorldInfoManager::load() {
    load(defaultServerInfoRepository());
}

void GameWorldInfoManager::load(ServerInfoRepository& repository) {
    std::vector<ServerInfoWorldRow> rows;
    try {
        rows = repository.loadWorlds();
    } catch (const DatabaseError& error) {
        throw Error("GameWorldInfoManager::load : " + error.message());
    }

    decltype(m_GameWorldInfos) replacement;
    cout << "Loading GameWorldInfoManager...." << endl;
    for (const auto& row : rows) {
        if (!std::in_range<WorldID_t>(row.id))
            throw Error("invalid world catalogue ID");
        if (row.stat < WORLD_OPEN || row.stat > WORLD_CLOSE)
            throw Error("invalid world status");

        GameWorldInfo info{};
        const auto worldID = static_cast<WorldID_t>(row.id);
        info.setID(worldID);
        info.setName(row.name);
        info.setStatus(static_cast<WorldStatus>(row.stat));
        cout << info.toString() << endl;
        cout << "Size : " << replacement.size() << endl;
        if (!replacement.emplace(worldID, std::move(info)).second)
            throw DuplicatedException("duplicated game-server nickname");
    }
    cout << "End GameWorldInfoManager Load" << endl;
    // Complete all throwing preparation/reporting before publication.
    m_GameWorldInfos.swap(replacement);
}

const GameWorldInfo* GameWorldInfoManager::getGameWorldInfo(WorldID_t worldID) const {
    const auto entry = m_GameWorldInfos.find(worldID);
    if (entry == m_GameWorldInfos.end())
        throw NoSuchElementException();
    return &entry->second;
}

string GameWorldInfoManager::toString() const {
    StringStream msg;
    msg << "GameWorldInfoManager(\n";
    if (m_GameWorldInfos.empty())
        msg << "EMPTY";
    else
        for (const auto& [id, info] : m_GameWorldInfos)
            msg << info.toString() << '\n';
    msg << ")";
    return msg.toString();
}
