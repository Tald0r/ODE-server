#include "LoginConnection.h"

namespace de {
void LoginConnectionDeleter::operator()(LoginPlayer* player) const noexcept {
    if (player) {
        player->setPlayerStatus(LPS_END_SESSION);
        delete player;
    }
}

LoginConnection makeLoginConnection(std::unique_ptr<Socket> socket) {
    if (!socket)
        return nullptr;
    LoginConnection player(new LoginPlayer(socket.release()));
    player->setPlayerStatus(LPS_BEGIN_SESSION);
    return player;
}
} // namespace de
