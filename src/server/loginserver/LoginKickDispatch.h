#ifndef DARKEDEN_LOGIN_KICK_DISPATCH_H
#define DARKEDEN_LOGIN_KICK_DISPATCH_H

#include <functional>
#include <string>

#include "Types.h"

class GameServerInfoManager;
class LGKickCharacter;
class LoginPlayer;

namespace de {

struct LoginKickTarget;
using LoginKickSend = std::function<void(const std::string& host, uint port, const LGKickCharacter& packet)>;

// Send a prepared target through explicit catalogue/transport dependencies.
// The sender borrows its arguments only during the call. Callers serialize this
// flow with player access, including kick verification.
// Prepare every occupied group's server-1 endpoint in ascending group order
// before sending. Missing server-1 rows or no destinations clear the identity
// without changing status/deadline. Other failures preserve session state.
// Waiting and its deadline follow all sender returns. A sender may already have
// sent earlier datagrams when a later call fails; retry starts a new broadcast.
// Return true after publication, false after destination refusal.
bool dispatchLoginKick(LoginPlayer& player, const LoginKickTarget& target, const GameServerInfoManager& servers,
                       const LoginKickSend& send);

} // namespace de

#endif
