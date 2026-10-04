#include "ZoneInfoManager.h"

#include <vector>

#include "DatabaseError.h"
#include "DiagnosticTrace.h"
#include "repository/LoginConfigRepository.h"

void ZoneInfoManager::init() {
    load();
    de::diagnosticTrace([&](std::ostream& output) { output << toString(); });
}

void ZoneInfoManager::load() {
    load(defaultLoginConfigRepository());
}

void ZoneInfoManager::load(LoginConfigRepository& repository) {
    std::vector<LoginZoneRow> rows;
    try {
        rows = repository.loadZones();
    } catch (const DatabaseError& error) {
        throw Error("ZoneInfoManager::load : " + error.message());
    }

    decltype(m_ZoneInfos) prepared;
    for (const auto& row : rows) {
        const auto [position, inserted] = prepared.try_emplace(row.zoneID);
        if (!inserted)
            throw DuplicatedException("duplicated zone id");
        position->second.setZoneID(row.zoneID);
        position->second.setZoneGroupID(row.zoneGroupID);
    }
    m_ZoneInfos.swap(prepared);
}

const ZoneInfo* ZoneInfoManager::getZoneInfo(ZoneID_t zoneID) const {
    const auto found = m_ZoneInfos.find(zoneID);
    if (found == m_ZoneInfos.end()) {
        StringStream message;
        message << "ZoneID : " << zoneID;
        throw NoSuchElementException(message.toString());
    }
    return &found->second;
}

string ZoneInfoManager::toString() const {
    StringStream message;
    message << "ZoneInfoManager(\n";
    if (m_ZoneInfos.empty())
        message << "EMPTY";
    else
        for (const auto& [id, zone] : m_ZoneInfos)
            message << zone.toString() << '\n';
    message << ")";
    return message.toString();
}
