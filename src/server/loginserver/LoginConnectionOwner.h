#ifndef DARKEDEN_LOGIN_CONNECTION_OWNER_H
#define DARKEDEN_LOGIN_CONNECTION_OWNER_H

#include <memory>

class LoginPlayer;

namespace de {

// Local teardown only; account logout remains an explicit action.
struct LoginConnectionDeleter {
    void operator()(LoginPlayer* player) const noexcept;
};
using LoginConnection = std::unique_ptr<LoginPlayer, LoginConnectionDeleter>;

} // namespace de

#endif
