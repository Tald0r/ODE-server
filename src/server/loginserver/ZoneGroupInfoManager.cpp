#include "ZoneGroupInfoManager.h"

#include <vector>

#include "DatabaseError.h"
#include "repository/LoginConfigRepository.h"

void ZoneGroupInfoManager::init() {
    load();
    cout << toString() << endl;
}

void ZoneGroupInfoManager::load() {
    load(defaultLoginConfigRepository());
}

void ZoneGroupInfoManager::load(LoginConfigRepository& repository) {
    std::vector<LoginZoneGroupRow> rows;
    try {
        rows = repository.loadZoneGroups();
    } catch (const DatabaseError& error) {
        throw Error("ZoneGroupInfoManager::load : " + error.message());
    }

    decltype(m_ZoneGroupInfos) prepared;
    for (const auto& row : rows) {
        const auto [position, inserted] = prepared.try_emplace(row.zoneGroupID);
        if (!inserted)
            throw DuplicatedException("duplicated zone id");
        position->second.setZoneGroupID(row.zoneGroupID);
        position->second.setServerID(row.serverID);
    }
    m_ZoneGroupInfos.swap(prepared);
}

const ZoneGroupInfo* ZoneGroupInfoManager::getZoneGroupInfo(ZoneGroupID_t zoneGroupID) const {
    const auto found = m_ZoneGroupInfos.find(zoneGroupID);
    if (found == m_ZoneGroupInfos.end()) {
        StringStream message;
        message << "ZoneGroupID : " << zoneGroupID;
        throw NoSuchElementException(message.toString());
    }
    return &found->second;
}

string ZoneGroupInfoManager::toString() const {
    StringStream message;
    message << "ZoneGroupInfoManager(";
    if (m_ZoneGroupInfos.empty())
        message << "EMPTY";
    else
        for (const auto& [id, group] : m_ZoneGroupInfos)
            message << group.toString();
    message << ")";
    return message.toString();
}
