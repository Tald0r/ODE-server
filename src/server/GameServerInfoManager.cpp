#include "GameServerInfoManager.h"

#include <utility>

#include "repository/ServerInfoRepository.h"

void GameServerInfoManager::Catalogue::swap(Catalogue& other) noexcept {
    rows.swap(other.rows);
    tables.swap(other.tables);
    view.swap(other.view);
    std::swap(groupCount, other.groupCount);
}

GameServerInfo* GameServerInfoManager::Catalogue::lookup(ServerID_t serverID, ServerGroupID_t groupID,
                                                         WorldID_t worldID) const {
    if (worldID >= tables.size() || groupID >= groupCount)
        throw NoSuchElementException();
    const auto& group = tables[worldID][groupID];
    const auto found = group.find(serverID);
    if (found == group.end())
        throw NoSuchElementException();
    return found->second;
}

void GameServerInfoManager::clear() noexcept {
    Catalogue empty;
    m_Catalogue.swap(empty);
}

void GameServerInfoManager::init() {
    load();
    cout << toString() << endl;
}

void GameServerInfoManager::load() {
    load(defaultServerInfoRepository());
}

void GameServerInfoManager::load(ServerInfoRepository& repository) {
    int maxGroup = 0;
    if (!repository.loadMaxServerGroupID(maxGroup)) {
        cerr << "GameServerInfo TABLE does not exist!" << endl;
        throw Error("GameServerInfo TABLE does not exist!");
    }
    if (!std::in_range<ServerGroupID_t>(maxGroup))
        throw Error("invalid maximum game-server GroupID");

    int maxWorld = 0;
    if (!repository.loadMaxWorldID(maxWorld)) {
        cerr << "GameServerInfo TABLE does not exist!" << endl;
        throw Error("GameServerInfo TABLE does not exist!");
    }
    if (!std::in_range<WorldID_t>(maxWorld))
        throw Error("invalid maximum game-server WorldID");

    Catalogue prepared;
    prepared.groupCount = maxGroup + 1; // Checked before arithmetic or indexing.
    prepared.tables.resize(static_cast<std::size_t>(maxWorld) + 2);
    prepared.view = std::make_unique<HashMapGameServerInfo*[]>(prepared.tables.size());
    for (std::size_t world = 0; world < prepared.tables.size(); ++world) {
        prepared.tables[world].resize(static_cast<std::size_t>(prepared.groupCount));
        prepared.view[world] = prepared.tables[world].data();
    }
    cout << "MAX SERVER GROUP = " << prepared.groupCount << endl;

    const auto servers = repository.loadServers();
    prepared.rows.reserve(servers.size());
    for (const auto& row : servers) {
        if (!std::in_range<ServerID_t>(row.serverID) || !std::in_range<WorldID_t>(row.worldID) ||
            row.worldID > maxWorld || !std::in_range<ServerGroupID_t>(row.groupID) || row.groupID > maxGroup)
            throw Error("invalid game-server catalogue ID");
        if (row.stat < SERVER_FREE || row.stat > SERVER_DOWN)
            throw Error("invalid game-server status");
        // Ports retain their stored unsigned width; endpoint policy belongs to transport setup.
        if (!std::in_range<uint>(row.tcpPort) || !std::in_range<uint>(row.udpPort))
            throw Error("invalid game-server catalogue port");

        auto info = std::make_unique<GameServerInfo>();
        info->setServerID(static_cast<ServerID_t>(row.serverID));
        info->setNickname(row.nickname);
        info->setIP(row.ip);
        info->setTCPPort(static_cast<uint>(row.tcpPort));
        info->setUDPPort(static_cast<uint>(row.udpPort));
        info->setWorldID(static_cast<WorldID_t>(row.worldID));
        info->setGroupID(static_cast<ServerGroupID_t>(row.groupID));
        info->setServerStat(static_cast<ServerStatus>(row.stat));
        auto* borrowed = info.get();
        prepared.rows.push_back(std::move(info));
        auto& group = prepared.tables[static_cast<std::size_t>(row.worldID)][static_cast<std::size_t>(row.groupID)];
        if (!group.emplace(borrowed->getServerID(), borrowed).second)
            throw DuplicatedException("duplicated game-server ServerID");
    }

    for (const auto& row : repository.loadNonPKServers()) {
        if (!std::in_range<WorldID_t>(row.worldID) || row.worldID > maxWorld ||
            !std::in_range<ServerGroupID_t>(row.serverGroupID) || row.serverGroupID > maxGroup)
            throw Error("invalid non-PK catalogue ID");
        prepared.lookup(1, static_cast<ServerGroupID_t>(row.serverGroupID), static_cast<WorldID_t>(row.worldID))
            ->setNonPKServer();
        cout << "WorldID:" << row.worldID << " ServerGroupID:" << row.serverGroupID << " NonPK set" << endl;
    }

    for (const auto& row : repository.loadCastleStats()) {
        if (!std::in_range<WorldID_t>(row.worldID) || row.worldID > maxWorld ||
            !std::in_range<ServerGroupID_t>(row.serverGroupID) || row.serverGroupID > maxGroup ||
            !std::in_range<ServerID_t>(row.followServerID))
            throw Error("invalid castle-following catalogue ID");
        prepared.lookup(1, static_cast<ServerGroupID_t>(row.serverGroupID), static_cast<WorldID_t>(row.worldID))
            ->setCastleFollowingServerID(static_cast<ServerID_t>(row.followServerID));
        cout << "WorldID:" << row.worldID << " ServerGroupID:" << row.serverGroupID << " follows" << row.followServerID
             << endl;
    }

    // No row, index, flag, dimension or diagnostic remains to prepare at publication.
    m_Catalogue.swap(prepared);
}

GameServerInfo* GameServerInfoManager::getGameServerInfo(ServerID_t serverID, ServerGroupID_t groupID,
                                                         WorldID_t worldID) const {
    return m_Catalogue.lookup(serverID, groupID, worldID);
}

uint GameServerInfoManager::getSize(WorldID_t worldID, ServerGroupID_t groupID) const noexcept {
    return worldID < m_Catalogue.tables.size() && groupID < m_Catalogue.groupCount
               ? m_Catalogue.tables[worldID][groupID].size()
               : 0;
}

string GameServerInfoManager::toString() const {
    StringStream message;
    message << "GameServerInfoManager(\n";
    for (const auto& world : m_Catalogue.tables) {
        for (const auto& group : world) {
            if (group.empty())
                message << "EMPTY";
            else
                for (const auto& [id, info] : group)
                    message << info->toString() << '\n';
            message << ")";
        }
    }
    return message.toString();
}
