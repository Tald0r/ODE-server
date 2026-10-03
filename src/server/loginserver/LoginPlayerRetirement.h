#ifndef DARKEDEN_LOGIN_PLAYER_RETIREMENT_H
#define DARKEDEN_LOGIN_PLAYER_RETIREMENT_H

#include <chrono>
#include <exception>
#include <functional>

#include "LoginConnectionOwner.h"

namespace de {

using LoginRetirementTime = std::chrono::steady_clock::time_point;
struct LoginRetirementActions {
    std::function<void(LoginPlayer&, bool flush)> disconnect;
    std::function<void(const std::exception_ptr&)> report;
};

const LoginRetirementActions& defaultLoginRetirementActions();

// Owns detached players whose acquired account still needs logout. Links live
// in each player, so adopting a retirement never allocates. Callers serialize
// access; callbacks must not reenter this queue. Local teardown is iterative and
// does not access the database, matching scoped manager destruction.
class LoginPlayerRetirement {
public:
    LoginPlayerRetirement() = default;
    ~LoginPlayerRetirement();
    LoginPlayerRetirement(const LoginPlayerRetirement&) = delete;
    LoginPlayerRetirement& operator=(const LoginPlayerRetirement&) = delete;

    // Disconnect, then guarantee END and local socket closure. Report the
    // initiating/cleanup errors best effort. Keep only outstanding account
    // owners; other players are destroyed even if flush or reporting failed.
    void retire(LoginConnection player, bool flush, LoginRetirementTime now, const LoginRetirementActions& actions,
                std::exception_ptr reason = {}) noexcept;
    // At most one attempt per due owner per call; failed owners wait five more
    // seconds on the supplied monotonic clock. Retries never flush closed sockets.
    void retry(LoginRetirementTime now, const LoginRetirementActions& actions) noexcept;
    void clear() noexcept;
    std::size_t size() const noexcept {
        return m_Size;
    }

private:
    LoginConnection m_Pending;
    std::size_t m_Size = 0;
};

} // namespace de

#endif
