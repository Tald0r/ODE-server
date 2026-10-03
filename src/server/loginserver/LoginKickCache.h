#ifndef DARKEDEN_LOGIN_KICK_CACHE_H
#define DARKEDEN_LOGIN_KICK_CACHE_H

#include <memory>
#include <string>

#include "Types.h"

namespace de {

struct LoginKickTarget {
    WorldID_t worldID = 0;
    ServerGroupID_t groupID = 0;
    // Account slots are one-based; zero means no previous slot.
    uint lastSlot = 0;
    std::string characterName;
    bool operator==(const LoginKickTarget&) const = default;
};

// One account's saved target, independent of the session's live selection.
// Borrowed pointers survive failed replacement and expire on store/clear.
// Access is serialized with the player; validation belongs to preparation.
class LoginKickCache {
public:
    const LoginKickTarget* find(const std::string& account) const noexcept {
        return m_Entry && m_Entry->account == account ? &m_Entry->target : nullptr;
    }

    void store(const std::string& account, const LoginKickTarget& target) {
        auto prepared = std::make_unique<Entry>(Entry{account, target});
        m_Entry.swap(prepared);
    }

    void clear() noexcept {
        m_Entry.reset();
    }

private:
    struct Entry {
        std::string account;
        LoginKickTarget target;
    };
    std::unique_ptr<Entry> m_Entry;
};

} // namespace de

#endif
