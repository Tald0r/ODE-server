#ifndef DARKEDEN_OUTBOUND_SERVER_CONNECTION_H
#define DARKEDEN_OUTBOUND_SERVER_CONNECTION_H

#include <cstdint>
#include <memory>
#include <string>

#include "Socket.h"

namespace de {

// Make one blocking TCP connection attempt, then configure nonblocking I/O
// and disable linger. Return ownership only after every step succeeds; failure
// releases the socket and propagates. Hosts keep Socket's numeric IPv4 policy.
// The caller validates configuration, owns retry/shutdown policy and transfers
// the completed socket to its protocol session.
std::unique_ptr<Socket> connectOutboundServer(const std::string& host, std::uint16_t port);

} // namespace de

#endif
