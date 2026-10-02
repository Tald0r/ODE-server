#ifndef DARKEDEN_GAME_CONNECTION_H
#define DARKEDEN_GAME_CONNECTION_H

#include <memory>

#include "GamePlayer.h"

namespace de {
// End a local session before destroying its resources. A caller that needs
// account/zone logout must explicitly disconnect before releasing this owner.
struct GameConnectionDeleter {
    void operator()(GamePlayer* player) const noexcept;
};

using GameConnection = std::unique_ptr<GamePlayer, GameConnectionDeleter>;

// Adopt an authorized, prepared socket even on failure. The begun session
// stays owned here until registration or a subsequent handoff succeeds.
GameConnection makeGameConnection(std::unique_ptr<Socket> socket);
} // namespace de

#endif
