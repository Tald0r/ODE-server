#ifndef DARKEDEN_LOGIN_CHARACTER_CREATION_H
#define DARKEDEN_LOGIN_CHARACTER_CREATION_H

#include <functional>

class CLCreatePC;
class CreatePCBalanceCache;
class LCCreatePCError;
class LCCreatePCOK;
class LoginCharacterRepository;
class LoginPlayer;

namespace de {

struct LoginCharacterCreationActions {
    std::function<void(LoginPlayer&, LCCreatePCError&)> sendRefusal;
    std::function<void(LoginPlayer&, LCCreatePCOK&)> sendSuccess;
};

const LoginCharacterCreationActions& defaultLoginCharacterCreationActions();

// Snapshot the packet/session, decide, then write the rows and send success.
// The packet's attributes are updated before the first write; the player's
// phase advances only after success sending returns. Earlier writes are not
// rolled back after a later failure. Normal refusals return false after sending.
// A DatabaseError from the decision, writes or initial reply triggers one generic
// refusal; errors sending that refusal propagate. Other errors retain identity.
// The caller owns the player and serializes access to it and the balance cache.
// Callbacks are synchronous and must not mutate or retire the player.
[[nodiscard]] bool createLoginCharacter(LoginPlayer& player, CLCreatePC& packet, LoginCharacterRepository& repository,
                                        CreatePCBalanceCache& balance, const LoginCharacterCreationActions& actions);

} // namespace de

#endif
