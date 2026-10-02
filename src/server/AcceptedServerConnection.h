#ifndef DARKEDEN_ACCEPTED_SERVER_CONNECTION_H
#define DARKEDEN_ACCEPTED_SERVER_CONNECTION_H

#include <memory>

#include "Socket.h"

namespace de {
// Takes ownership, including on failure. An empty nonblocking accept remains
// empty. Check socket errors around nonblocking setup, then disable linger;
// return the socket only after preparation succeeds. The caller owns endpoint
// admission, diagnostics and transfer to its protocol session.
std::unique_ptr<Socket> prepareAcceptedServerConnection(std::unique_ptr<Socket> socket);
} // namespace de

#endif
