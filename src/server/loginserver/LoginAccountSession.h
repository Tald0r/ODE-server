#ifndef DARKEDEN_LOGIN_ACCOUNT_SESSION_H
#define DARKEDEN_LOGIN_ACCOUNT_SESSION_H

#include <functional>
#include <string>

class LoginPlayer;
class LoginAccountRepository;
struct LoginNewAccount;

namespace de {

class LoginAccountOwnership;

struct LoginDisconnectActions {
    std::function<void()> flush;
    std::function<void()> close;
};

// Prepare ownership before either registration write and publish it only after
// both return. A failed second write can leave an inserted, unacquired row.
void registerLoginAccount(LoginAccountOwnership& ownership, LoginAccountRepository& accounts,
                          const LoginNewAccount& account, const std::string& ip, int loginServerID);
// Always attempt close and logout despite earlier failures. END and suppressed
// identity precede logout; the first exception is rethrown unchanged after all
// cleanup attempts. Failed logout retains ownership for a later call. Repeated
// calls retry close/failed logout, never flush END or repeat successful logout.
// Callers serialize access. Production adapters keep DatabaseError translation.
void disconnectLoginPlayer(LoginPlayer& player, LoginAccountRepository& accounts, bool flush,
                           const LoginDisconnectActions& actions);

} // namespace de

#endif
