#ifndef DARKEDEN_LOGIN_ACCOUNT_NAME_QUERY_H
#define DARKEDEN_LOGIN_ACCOUNT_NAME_QUERY_H

#include <functional>

class CLQueryPlayerID;
class LCQueryResultPlayerID;
class LoginAccountRepository;
class LoginPlayer;

namespace de {

struct LoginAccountNameQueryActions {
    std::function<void(LoginPlayer&, LCQueryResultPlayerID&)> send;
};

const LoginAccountNameQueryActions& defaultLoginAccountNameQueryActions();

// Own the requested account name before lookup and reply using that same name.
// Every call performs the account-name probe; no character-name policy applies.
// Publish the registration phase only after sending returns. Repository/send
// failures propagate unchanged, retaining the session and any partial output.
// Callers serialize access and retain player/account ownership. The synchronous
// sender must not mutate or retire the player. Decoded string bounds and packet
// admission remain the caller's contract.
void queryLoginAccountName(LoginPlayer& player, const CLQueryPlayerID& packet, LoginAccountRepository& repository,
                           const LoginAccountNameQueryActions& actions);

} // namespace de

#endif
