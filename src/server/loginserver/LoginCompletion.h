#ifndef DARKEDEN_LOGIN_COMPLETION_H
#define DARKEDEN_LOGIN_COMPLETION_H

#include <functional>
#include <string>

#include "Types.h"

class LCLoginOK;
class LoginAccountRepository;
class LoginPlayer;
class Throwable;
class VSDateTime;

namespace de {

using LoginStatisticsClock = std::function<VSDateTime()>;
struct LoginCompletionDiagnostics {
    std::function<void(const std::string& account, const std::string& ip)> refused;
    std::function<void(const Throwable&)> failed;
};

// Both login paths initialize every reply field; the unused status byte is zero.
LCLoginOK makeLoginOK(bool adult, bool family, WORD lastDays);
// Keep the existing local date/time split. Caller supplies the timestamp and
// repository; query/formatting failures propagate without translation.
void recordLogin(LoginAccountRepository& accounts, const std::string& account, const std::string& ip,
                 const VSDateTime& now);
// The caller admits the pending kick and serializes session access. Snapshot
// reply inputs before account writes. Success orders LOGON, IP, reply, phase,
// clock and statistics. Refusal suppresses identity and returns to BEGIN only
// after its reply succeeds. Return true only after statistics return normally.
// This is not a database transaction: successful writes/sends and the published
// phase survive later failures. Diagnostics are best effort; Throwable failures
// are reported, and all exception types propagate with their original identity.
bool completeLoginKick(LoginPlayer& player, LoginAccountRepository& accounts, const LoginStatisticsClock& clock,
                       const LoginCompletionDiagnostics& diagnostics);

} // namespace de

#endif
