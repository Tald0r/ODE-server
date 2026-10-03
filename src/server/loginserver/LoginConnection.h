#ifndef DARKEDEN_LOGIN_CONNECTION_H
#define DARKEDEN_LOGIN_CONNECTION_H

#include <memory>

#include "LoginConnectionOwner.h"
#include "LoginPlayer.h"

namespace de {
// Adopt a prepared socket, including on failure, and begin its session.
// Keep the returned owner until the player table accepts registration.
LoginConnection makeLoginConnection(std::unique_ptr<Socket> socket);
} // namespace de

#endif
