#ifndef DARKEDEN_LOGIN_CONNECTION_H
#define DARKEDEN_LOGIN_CONNECTION_H

#include <memory>

#include "LoginPlayer.h"

namespace de {
// Local resource teardown: end the session before its history, streams and
// socket are destroyed. Account logout remains an explicit disconnect action.
struct LoginConnectionDeleter {
    void operator()(LoginPlayer* player) const noexcept;
};

using LoginConnection = std::unique_ptr<LoginPlayer, LoginConnectionDeleter>;

// Adopt a prepared socket, including on failure, and begin its session.
// Keep the returned owner until the player table accepts registration.
LoginConnection makeLoginConnection(std::unique_ptr<Socket> socket);
} // namespace de

#endif
