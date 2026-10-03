#ifndef DARKEDEN_LOGIN_CHARACTER_NAME_QUERY_H
#define DARKEDEN_LOGIN_CHARACTER_NAME_QUERY_H

#include <functional>

class CLQueryCharacterName;
class GameWorldInfoManager;
class LCQueryResultCharacterName;
class LoginCharacterRepository;
class LoginPlayer;

namespace de {

struct LoginCharacterNameQueryActions {
    std::function<void(LoginPlayer&, LCQueryResultCharacterName&)> send;
};

const LoginCharacterNameQueryActions& defaultLoginCharacterNameQueryActions();

// Validate actual world membership and own the queried name before callbacks.
// Query once even for reserved names; reserved tokens are then reported as taken.
// Publish the next phase only after sending returns. Missing worlds retain the
// assertion failure boundary; repository/send failures propagate unchanged.
// Callers serialize player access, retain its ownership and keep the catalogue
// alive without concurrent reloads. The synchronous sender must not mutate or
// retire the player. Decoded packet string limits remain the caller's contract.
void queryLoginCharacterName(LoginPlayer& player, const CLQueryCharacterName& packet,
                             const GameWorldInfoManager& worlds, LoginCharacterRepository& repository,
                             const LoginCharacterNameQueryActions& actions);

} // namespace de

#endif
