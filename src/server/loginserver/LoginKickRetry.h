#ifndef DARKEDEN_LOGIN_KICK_RETRY_H
#define DARKEDEN_LOGIN_KICK_RETRY_H

#include <functional>

#include "Timeval.h"

class LoginPlayer;

namespace de {

// A successful request publishes its waiting phase and next deadline. False
// means refusal; the action owns its response and session changes. Exceptions
// propagate, including after a partial broadcast. Callers serialize player access.
using LoginKickRequest = std::function<bool(LoginPlayer&)>;

// An authenticated fresh attempt resets the retry count before requesting its
// first kick, including when that request refuses or throws. Cached routing and
// the previous deadline remain intact until the request changes them.
bool beginLoginKick(LoginPlayer& player, const LoginKickRequest& send);

// At or after the deadline, resend only for a waiting, account-bound target.
// Count a successful resend only while the same account, target and local
// attempt remain pending. Complete after the third retry (four sends in all).
// The count saturates at three; if completion throws without ending the wait,
// the next deadline permits another resend/completion. Actions own mutations;
// neither refusal nor an exception is rolled back. True means a retry was
// counted and any required completion returned normally.
bool retryLoginKick(LoginPlayer& player, const Timeval& now, const LoginKickRequest& send,
                    const std::function<void(LoginPlayer&)>& complete);

} // namespace de

#endif
