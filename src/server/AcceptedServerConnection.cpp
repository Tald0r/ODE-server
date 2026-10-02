#include "AcceptedServerConnection.h"

namespace de {
std::unique_ptr<Socket> prepareAcceptedServerConnection(std::unique_ptr<Socket> socket) {
    if (!socket)
        return nullptr;
    if (socket->getSockError())
        throw Error();
    socket->setNonBlocking(true);
    if (socket->getSockError())
        throw Error();
    socket->setLinger(0);
    return socket;
}
} // namespace de
