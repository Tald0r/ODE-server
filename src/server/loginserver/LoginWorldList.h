#ifndef DARKEDEN_LOGIN_WORLD_LIST_H
#define DARKEDEN_LOGIN_WORLD_LIST_H

class GameWorldInfoManager;
class LoginAccountRepository;
class Player;

namespace de {

// Build and synchronously send the world-list reply from explicit inputs.
// The catalogue must remain alive and quiescent for this operation.
// Send receives a complete reply that remains owned until the call returns or throws.
void sendLoginWorldList(Player& player, const GameWorldInfoManager& worlds, LoginAccountRepository& accounts);

} // namespace de

#endif
