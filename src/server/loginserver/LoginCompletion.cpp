#include "LoginCompletion.h"

#include "LCLoginError.h"
#include "LCLoginOK.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "VSDateTime.h"
#include "repository/LoginAccountRepository.h"

namespace de {

LCLoginOK makeLoginOK(bool adult, bool family, WORD lastDays) {
    LCLoginOK reply;
    reply.setAdult(adult);
    reply.setFamily(family);
    reply.setStat(0);
    reply.setLastDays(lastDays);
    return reply;
}

void recordLogin(LoginAccountRepository& accounts, const std::string& account, const std::string& ip,
                 const VSDateTime& now) {
    const auto current = now.toDateTime();
    accounts.insertLoginRecord(account, ip, current.substr(0, 10), current.substr(11));
}

bool completeLoginKick(LoginPlayer& player, LoginAccountRepository& accounts, const LoginStatisticsClock& clock,
                       const LoginCompletionDiagnostics& diagnostics) {
    try {
        const auto account = player.getID();
        const auto ip = player.getSocket()->getHost();
        auto reply = makeLoginOK(player.isAdult(), false, 0xffff);
        auto& ownership = player.loginAccountOwnership();
        if (!ownership.owns(account) && !ownership.acquire(account, [&] { return accounts.setLoggedOn(account); })) {
            try {
                if (diagnostics.refused)
                    diagnostics.refused(account, ip);
            } catch (...) {
            }
            LCLoginError error;
            error.setErrorID(ALREADY_CONNECTED);
            player.sendPacket(&error);
            // No LOGON row was acquired. Disconnect must not log off another session.
            player.setID("NONE");
            player.setPlayerStatus(LPS_BEGIN_SESSION);
            return false;
        }
        accounts.setLoginIP(ip, account);
        player.sendPacket(&reply);
        player.setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        recordLogin(accounts, account, ip, clock());
        return true;
    } catch (const Throwable& error) {
        try {
            if (diagnostics.failed)
                diagnostics.failed(error);
        } catch (...) {
        }
        throw;
    }
}

} // namespace de
