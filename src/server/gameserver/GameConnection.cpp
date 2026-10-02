#include "GameConnection.h"

namespace de {
void GameConnectionDeleter::operator()(GamePlayer* player) const noexcept {
    if (player) {
        player->setPlayerStatus(GPS_END_SESSION);
        delete player;
    }
}

GameConnection makeGameConnection(std::unique_ptr<Socket> socket) {
    if (!socket)
        return nullptr;
    GameConnection player(new GamePlayer(socket.release()));
    player->setPlayerStatus(GPS_BEGIN_SESSION);
    return player;
}
} // namespace de
