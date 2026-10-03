#ifndef DARKEDEN_LOGIN_AUTHENTICATION_H
#define DARKEDEN_LOGIN_AUTHENTICATION_H

#include <functional>
#include <string>

#include <string_view>

#include "LoginDecision.h"
#include "PasswordHash.h"

class CLLogin;
class LCLoginError;
class LoginPlayer;

namespace de {

struct LoginAuthenticationActions {
    std::function<password::Verify(std::string_view, std::string_view)> verifyPassword;
    std::function<std::string(std::string_view)> hashPassword;
    std::function<void(LoginPlayer&, LCLoginError&)> sendRefusal;
    std::function<void(const std::string&, const char*, int)> refusal;
    // Hash implementations must not include credentials in their error details.
    std::function<void(const std::string&, const char*, bool)> hashingFailure;
    std::function<void(const std::string&)> newNetMarbleAccount;
    WebLoginKeyActions webKey;
};

const LoginAuthenticationActions& defaultLoginAuthenticationActions();

// Own the reported account before sending. Reply exceptions propagate; the
// optional diagnostic runs only after sending and must not change the result.
void sendLoginRefusal(LoginPlayer& player, std::string account, LoginRejectReason reason,
                      const LoginAuthenticationActions& actions);

// Own account and credential before collaborators run. These operations do not
// acquire LOGON ownership or publish identity/phase. The caller admits packets,
// serializes access and retains the player; callbacks are synchronous and may
// not mutate/retire it or retain borrowed arguments. Diagnostics are optional,
// best effort, and receive no passwords or web keys.
// NetMarble verification may migrate a hash or insert an account. Web acceptance
// consumes the key before granting a free pass. Earlier writes survive later
// failure, including an effect whose write acknowledgement throws.
// Throwable failures in verification/consumption return false; other operation
// exceptions propagate. NetMarble refusal sending lies outside that catch.
[[nodiscard]] bool verifyNetMarblePassword(const CLLogin& packet, LoginAccountRepository& repository,
                                           const LoginAuthenticationActions& actions);
[[nodiscard]] bool authorizeNetMarbleLogin(LoginPlayer& player, const CLLogin& packet,
                                           LoginAccountRepository& repository,
                                           const LoginAuthenticationActions& actions);
[[nodiscard]] bool authorizeWebLogin(LoginPlayer& player, const CLLogin& packet, LoginAccountRepository& repository,
                                     const LoginAuthenticationActions& actions);

} // namespace de

#endif
