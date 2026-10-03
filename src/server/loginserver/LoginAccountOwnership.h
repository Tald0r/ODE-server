#ifndef DARKEDEN_LOGIN_ACCOUNT_OWNERSHIP_H
#define DARKEDEN_LOGIN_ACCOUNT_OWNERSHIP_H

#include <optional>
#include <string>
#include <utility>

#include "Exception.h"

namespace de {

// Owns the identity of one successfully acquired LOGON row independently of
// the player's current ID and phase. Destruction releases local storage only;
// database cleanup is explicit. Callers serialize access and callbacks must not
// reenter this owner. A repository exception does not acknowledge acquisition.
class LoginAccountOwnership {
public:
    LoginAccountOwnership() = default;
    LoginAccountOwnership(const LoginAccountOwnership&) = delete;
    LoginAccountOwnership& operator=(const LoginAccountOwnership&) = delete;

    const std::string* account() const noexcept {
        return m_Account ? &*m_Account : nullptr;
    }
    bool owns(const std::string& account) const noexcept {
        return m_Account && *m_Account == account;
    }

    // Prepare identity storage before the write. Publication after a successful
    // write moves the string without allocating. Refusal/failure leaves no owner;
    // an existing owner must be released before a new acquisition can start.
    template <typename Acquire> bool acquire(const std::string& account, Acquire&& write) {
        if (m_Account)
            throw DisconnectException("login account already acquired");
        std::string prepared(account);
        if (!std::forward<Acquire>(write)())
            return false;
        m_Account.emplace(std::move(prepared));
        return true;
    }

    // Failed logout retains identity and borrowed pointers so cleanup can retry.
    // Once successful, repeated release is a no-op.
    template <typename Release> bool release(Release&& write) {
        if (!m_Account)
            return false;
        std::forward<Release>(write)(*m_Account);
        m_Account.reset();
        return true;
    }

private:
    std::optional<std::string> m_Account;
};

} // namespace de

#endif
