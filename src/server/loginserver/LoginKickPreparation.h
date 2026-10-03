#ifndef DARKEDEN_LOGIN_KICK_PREPARATION_H
#define DARKEDEN_LOGIN_KICK_PREPARATION_H

#include <optional>

#include "LoginKickCache.h"

class LoginAccountRepository;
class LoginCharacterRepository;
class LoginPlayer;

namespace de {

// Resolve the account's previous location and character using supplied reads.
// A complete target is cached before return. Missing data sends ALREADY_CONNECTED
// and returns no target; successful refusal resets the session and clears its ID.
// Query/validation/allocation failures leave the cache intact. Invalid IDs or
// slots throw Error; slot zero requires an already known character name.
// A matching cache keeps its saved world independently of live selection.
[[nodiscard]] std::optional<LoginKickTarget> prepareLoginKick(LoginPlayer& player, LoginAccountRepository& accounts,
                                                              LoginCharacterRepository& characters);

} // namespace de

#endif
