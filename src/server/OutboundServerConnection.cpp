#include "OutboundServerConnection.h"

namespace de {

std::unique_ptr<Socket> connectOutboundServer(const std::string& host, std::uint16_t port) {
    auto socket = std::make_unique<Socket>(host, port);
    socket->connect();
    socket->setNonBlocking(true);
    socket->setLinger(0);
    return socket;
}

} // namespace de
