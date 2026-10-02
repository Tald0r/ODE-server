///////////////////////////////////////////////////////////
// Filename : StringPool.cpp
///////////////////////////////////////////////////////////

#include "StringPool.h"

#include <utility>

#include "repository/SharedConfigRepository.h"

void StringPool::load() {
    load(defaultSharedConfigRepository());
}

void StringPool::load(SharedConfigRepository& repository) {
    decltype(m_Strings) replacement;
    for (const auto& row : repository.loadStrings()) {
        if (!std::in_range<uint>(row.id))
            throw Error("invalid shared-string ID");
        if (!replacement.try_emplace(static_cast<uint>(row.id), row.text).second)
            throw DuplicatedException("StringPool::addString()");
    }
    m_Strings.swap(replacement);
}

string StringPool::getString(uint strID) const {
    const auto entry = m_Strings.find(strID);
    if (entry == m_Strings.end())
        throw NoSuchElementException("StringPool::getStrind()");
    return entry->second;
}

const char* StringPool::c_str(uint strID) const {
    const auto entry = m_Strings.find(strID);
    if (entry == m_Strings.end())
        throw NoSuchElementException("StringPool::getStrind()");
    return entry->second.c_str();
}
