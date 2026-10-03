#ifndef DARKEDEN_LOGIN_CHARACTER_LIST_REFRESH_H
#define DARKEDEN_LOGIN_CHARACTER_LIST_REFRESH_H

#include <functional>

class LCPCList;
class LoginCharacterRepository;
class LoginPlayer;

namespace de {

struct LoginCharacterListRefreshActions {
    std::function<void(LoginPlayer&, LCPCList&)> send;
};

const LoginCharacterListRefreshActions& defaultLoginCharacterListRefreshActions();

// Own the selected world/account before querying and keep the complete reply
// alive through synchronous sending. Publish character management only after
// sending returns; failures preserve the original phase and any partial output.
// Assembly retains its database/missing-row exception policy. Sender exceptions
// propagate unchanged. Callers serialize access and retain player/account
// ownership; the sender may not retain the reply or mutate/retire the player.
// Packet admission remains the caller's contract.
void refreshLoginCharacterList(LoginPlayer& player, LoginCharacterRepository& repository,
                               const LoginCharacterListRefreshActions& actions);

} // namespace de

#endif
