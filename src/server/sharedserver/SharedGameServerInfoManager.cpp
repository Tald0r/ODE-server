//////////////////////////////////////////////////////////////////////////////
// Filename    : SharedGameServerInfoManager.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SharedGameServerInfoManager.h"

#include <limits>

#include "DiagnosticTrace.h"
#include "KernelContext.h"
#include "Properties.h"
#include "repository/SharedConfigRepository.h"

void SharedGameServerInfoManager::init() {
    load();
    de::diagnosticTrace([&](std::ostream& output) { output << toString(); });
}

void SharedGameServerInfoManager::load() {
    load(defaultSharedConfigRepository(), de::kernelContext().config().getPropertyInt("WorldID"));
}

void SharedGameServerInfoManager::load(SharedConfigRepository& repo, int worldID) {
    if (worldID < 0 || worldID > std::numeric_limits<WorldID_t>::max())
        throw Error("invalid game-server WorldID");

    int maxGroupID = 0;
    if (!repo.loadMaxGameServerGroupID(worldID, maxGroupID))
        throw Error("GameServerInfo TABLE does not exist!");
    if (maxGroupID < 0 || maxGroupID > std::numeric_limits<ServerGroupID_t>::max())
        throw Error("invalid maximum game-server GroupID");

    std::vector<Servers> replacement(static_cast<std::size_t>(maxGroupID) + 1);
    de::diagnosticTrace([&](std::ostream& output) { output << "MAX SERVER GROUP = " << replacement.size(); });
    for (const auto& row : repo.loadGameServers()) {
        if (row.worldID != worldID)
            continue;
        if (row.groupID < 0 || row.groupID > maxGroupID || row.serverID < 0 ||
            row.serverID > std::numeric_limits<ServerID_t>::max())
            throw Error("invalid game-server catalogue ID");
        if (row.stat < SERVER_FREE || row.stat > SERVER_DOWN)
            throw Error("invalid game-server status");

        auto& group = replacement[static_cast<std::size_t>(row.groupID)];
        auto [entry, inserted] = group.try_emplace(static_cast<ServerID_t>(row.serverID));
        if (!inserted)
            throw DuplicatedException("duplicated game-server ServerID");
        auto& info = entry->second;
        info.setServerID(static_cast<ServerID_t>(row.serverID));
        info.setNickname(row.nickname);
        info.setIP(row.ip);
        info.setTCPPort(row.tcpPort);
        info.setUDPPort(row.udpPort);
        info.setWorldID(static_cast<WorldID_t>(worldID));
        info.setGroupID(static_cast<ServerGroupID_t>(row.groupID));
        info.setServerStat(static_cast<ServerStatus>(row.stat));
    }
    // Publication cannot throw; the temporary releases the previous catalogue.
    m_Servers.swap(replacement);
}

const SharedGameServerInfo* SharedGameServerInfoManager::getGameServerInfo(ServerID_t serverID,
                                                                           ServerGroupID_t groupID) const {
    if (groupID >= m_Servers.size())
        throw NoSuchElementException();
    const auto& group = m_Servers[groupID];
    const auto entry = group.find(serverID);
    if (entry == group.end())
        throw NoSuchElementException();
    return &entry->second;
}

uint SharedGameServerInfoManager::getSize(ServerGroupID_t groupID) const {
    if (groupID >= m_Servers.size())
        throw NoSuchElementException();
    return m_Servers[groupID].size();
}

string SharedGameServerInfoManager::toString() const {
    StringStream msg;
    msg << "GameServerInfoManager(\n";
    for (const auto& group : m_Servers) {
        if (group.empty())
            msg << "EMPTY";
        else
            for (const auto& [id, info] : group)
                msg << info.toString() << '\n';
        msg << ")";
    }
    return msg.toString();
}
