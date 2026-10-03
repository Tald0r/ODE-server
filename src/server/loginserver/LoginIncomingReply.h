#ifndef DARKEDEN_LOGIN_INCOMING_REPLY_H
#define DARKEDEN_LOGIN_INCOMING_REPLY_H

#include <functional>

#include "LoginPlayerRetirement.h"

class GLIncomingConnectionOK;
class GLIncomingConnectionError;
class LCReconnect;
class LoginPlayerManager;

namespace de {

struct LoginIncomingReplyActions {
    std::function<void(LoginPlayer&, LCReconnect&)> send;
    LoginRetirementActions retirement;
};

const LoginIncomingReplyActions& defaultLoginIncomingReplyActions();

// Return false for missing accounts or a session outside the pending phase.
// An admitted reply consumes the session: reconnect preparation/send failures
// are reported through retirement and close without flushing a partial reply.
// The manager stays locked from lookup through sending and retirement; actions
// must not reenter it or transfer the player's ownership. Lookup/lock failures
// propagate without consuming a player. Time controls failed-logout retries.
// There is no request nonce on the wire: a reply for an identical account in a
// later pending attempt cannot be distinguished by this protocol.
[[nodiscard]] bool completeLoginIncomingConnection(LoginPlayerManager& players, const GLIncomingConnectionOK& reply,
                                                   LoginRetirementTime now, const LoginIncomingReplyActions& actions);
[[nodiscard]] bool refuseLoginIncomingConnection(LoginPlayerManager& players, const GLIncomingConnectionError& reply,
                                                 LoginRetirementTime now, const LoginIncomingReplyActions& actions);

} // namespace de

#endif
