#ifndef DARKEDEN_LOGIN_FLOW_H
#define DARKEDEN_LOGIN_FLOW_H

#include <functional>
#include <string>

#include "LoginAuthentication.h"
#include "VSDateTime.h"

class CLLogin;
class LCLoginOK;
class LoginAccountRepository;
class LoginPlayer;

namespace de {

struct LoginFlowConfiguration {
    int loginServerID;
    bool useNetMarbleAdultFlag;
};

struct LoginFlowActions {
    LoginAuthenticationActions authentication;
    std::function<LoginFlowConfiguration()> configuration;
    std::function<VSDateTime()> clock;
    std::function<PasswordCheck(const std::string&, const std::string&, LoginAccountRepository&)> password;
    std::function<void(LoginPlayer&)> kick;
    std::function<void(LoginPlayer&, LCLoginOK&)> sendSuccess;
};

const LoginFlowActions& defaultLoginFlowActions();

// Retain the legacy gate's boundary: ProtocolException and Error propagate;
// other Exception failures are reported and let the caller continue. Standard
// exceptions outside the Throwable hierarchy and diagnostic failures propagate.
bool checkNetMarbleLoginGate(LoginPlayer& player, const CLLogin& packet, LoginAccountRepository& repository,
                             const LoginAuthenticationActions& actions);

// Clear prior authorization before preparing owned packet inputs; only the
// current authentication gate can grant a free pass. Normalize IDs back to the packet.
// Address and authentication gates precede configuration and ordinary password
// migration. Acquisition belongs to the session even if later writes or sending
// fail; sending precedes the successful phase transition. Kick dispatch retains
// its own completion and statistics policy. DatabaseError translation applies
// only to the decision/password/reply portion, not the gates or final statistics.
// Callers admit packets, serialize access and retain player/account ownership.
// Actions are synchronous and must not mutate/retire the player or retain borrowed
// arguments. Operations propagate failures except for the legacy NetMarble gate
// boundary above. Authentication diagnostics retain their optional best-effort
// contract.
void loginPlayer(LoginPlayer& player, CLLogin& packet, LoginAccountRepository& repository,
                 const LoginFlowActions& actions);

} // namespace de

#endif
