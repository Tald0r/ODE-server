///////////////////////////////////////////////////////////
// Filename : StringPool.cpp
///////////////////////////////////////////////////////////

#include "StringPool.h"

#include <utility>

#include "repository/GameInfoRepository.h"

StringPool::StringPool()

{}

StringPool::~StringPool()

{}

void StringPool::load()

{
    vector<StringPoolRow> rows = defaultGameInfoRepository().loadStrings();

    // Build the whole table before publishing it: a reader must never see a
    // half-filled pool, and a duplicate row leaves the pool it already has.
    StringHashMap next;

    for (size_t r = 0; r < rows.size(); r++) {
        uint strID = rows[r].id;

        if (next.contains(strID)) {
            throw DuplicatedException("StringPool::load()");
        }

        next[strID] = rows[r].text;
    }

    // Keep the map being replaced: a reader may hold a const char* into one
    // of its strings. The mutex spans the swap, so two reloads cannot both
    // retain the same old map and let the one between them go.
    std::lock_guard<std::mutex> lock(m_RetainedMutex);
    std::shared_ptr<const StringHashMap> replaced = m_Strings.load();

    m_Strings.update([&next](StringHashMap& table) { table = std::move(next); });

    m_RetainedStrings.push_back(std::move(replaced));
}

string StringPool::getString(uint strID) const {
    std::shared_ptr<const StringHashMap> strings = m_Strings.load();

    StringHashMapConstItor itr = strings->find(strID);

    if (itr == strings->end()) {
        throw NoSuchElementException("StringPool::getString()");
    }

    return itr->second;
}

const char* StringPool::c_str(uint strID) const {
    std::shared_ptr<const StringHashMap> strings = m_Strings.load();

    StringHashMapConstItor itr = strings->find(strID);

    if (itr == strings->end()) {
        throw NoSuchElementException("StringPool::getString()");
    }

    return itr->second.c_str();
}
