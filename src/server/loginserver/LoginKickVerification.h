#ifndef DARKEDEN_LOGIN_KICK_VERIFICATION_H
#define DARKEDEN_LOGIN_KICK_VERIFICATION_H

#include <functional>

class GLKickVerify;
class LoginPlayer;
class LoginPlayerManager;

namespace de {

using LoginKickCompletion = std::function<void(LoginPlayer&)>;

// Admit only a waiting session whose current account owns the named kick target.
// Missing/out-of-range descriptors and mismatches return false without completion.
// The manager owns the player and remains locked throughout completion. Completion
// owns its session changes; exceptions propagate unchanged after releasing the lock.
// The packet has no attempt nonce: identical replies for a later pending attempt
// on the same descriptor/name cannot be distinguished by this protocol.
[[nodiscard]] bool verifyLoginKick(LoginPlayerManager& players, const GLKickVerify& packet,
                                   const LoginKickCompletion& complete);

} // namespace de

#endif
