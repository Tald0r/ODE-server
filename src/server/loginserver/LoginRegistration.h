#ifndef DARKEDEN_LOGIN_REGISTRATION_H
#define DARKEDEN_LOGIN_REGISTRATION_H

#include <functional>
#include <string>

#include "Registration.h"

class CLRegisterPlayer;
class LCRegisterPlayerError;
class LCRegisterPlayerOK;
class LoginPlayer;

namespace de {

struct LoginRegistrationConnection {
    std::string ip;
    int loginServerID;
};

struct LoginRegistrationActions {
    RegisterPlayerActions decision;
    std::function<LoginRegistrationConnection(LoginPlayer&)> connection;
    std::function<std::string(WorldID_t, ServerGroupID_t)> groupName;
    std::function<void(LoginPlayer&, LCRegisterPlayerError&)> sendRefusal;
    std::function<void(LoginPlayer&, LCRegisterPlayerOK&)> sendSuccess;
    std::function<void(const std::string&, const std::string&)> hashingFailure;
};

const LoginRegistrationActions& defaultLoginRegistrationActions();

// Own packet fields before callbacks and connection settings before persistence.
// Successful insert/LOGON acquisition retains its cleanup owner after later
// failure. Earlier writes are not rolled back; an inserted account is not a
// fresh registration on retry. The checked location and identity are published
// without allocation only after the success sender returns.
// Ordinary refusals disconnect after sending; duplicate IDs, missing read-back
// and DatabaseError send a refusal and count up to three retryable failures.
// A DatabaseError from an initial reply gets one generic fallback; failure in
// that fallback propagates. Hashing diagnostics are optional and best effort.
// Callers serialize access and retain player/account ownership. Callbacks are
// synchronous and must not mutate/retire the player or retain reply references.
// Admission and decoded packet string limits remain the caller's contract.
[[nodiscard]] bool registerLoginPlayer(LoginPlayer& player, const CLRegisterPlayer& packet,
                                       LoginAccountRepository& repository, const LoginRegistrationActions& actions);

} // namespace de

#endif
