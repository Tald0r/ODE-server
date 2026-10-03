#include "LoginAccountSession.h"

#include <exception>

#include "LoginPlayer.h"
#include "repository/LoginAccountRepository.h"

namespace de {

void registerLoginAccount(LoginAccountOwnership& ownership, LoginAccountRepository& accounts,
                          const LoginNewAccount& account, const std::string& ip, int loginServerID) {
    ownership.acquire(account.playerID, [&] {
        accounts.insertAccount(account);
        accounts.markLoggedOnAfterRegister(ip, loginServerID, account.playerID);
        return true;
    });
}

void disconnectLoginPlayer(LoginPlayer& player, LoginAccountRepository& accounts, bool flush,
                           const LoginDisconnectActions& actions) {
    std::exception_ptr failure;
    const auto attempt = [&](const auto& action) {
        try {
            action();
        } catch (...) {
            if (!failure)
                failure = std::current_exception();
        }
    };
    if (flush && player.getPlayerStatus() != LPS_END_SESSION)
        attempt(actions.flush);
    attempt(actions.close);
    player.setPlayerStatus(LPS_END_SESSION);
    attempt([&] { player.setID("NONE"); });
    attempt([&] {
        player.loginAccountOwnership().release([&](const std::string& account) { accounts.markLoggedOff(account); });
    });
    if (failure)
        std::rethrow_exception(failure);
}

} // namespace de
