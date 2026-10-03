#include "LoginDatagramSend.h"

#include <limits>
#include <ostream>

#include "Datagram.h"
#include "DatagramPacket.h"

namespace de {

void validateLoginDatagramEndpoint(const std::string& host, uint port) {
    if (port == 0 || port > std::numeric_limits<WORD>::max())
        throw Error("login datagram port must be from 1 to 65535");
    in_addr address{};
    char canonical[INET_ADDRSTRLEN];
    // Some platforms accept leading zeroes in inet_pton, while the legacy
    // Datagram setter parses them as octal. Require identical decimal text.
    if (host.find('\0') != std::string::npos || ::inet_pton(AF_INET, host.c_str(), &address) != 1 ||
        ::inet_ntop(AF_INET, &address, canonical, sizeof(canonical)) == nullptr || host != canonical)
        throw Error("login datagram host must be a complete IPv4 address");
}

bool sendLoginDatagram(const std::string& host, uint port, const DatagramPacket& packet,
                       const LoginDatagramTransport& transport, std::ostream& errors) {
    try {
        validateLoginDatagramEndpoint(host, port);
        Datagram datagram;
        datagram.setHost(host);
        datagram.setPort(port);
        datagram.write(&packet);
        const auto expected = datagram.getLength();
        if (transport(datagram) != expected)
            throw Error("login datagram transport did not send the complete frame");
        return true;
    } catch (const Throwable& error) {
        // A failed formatter or stream cannot turn a failed send into success
        // or replace the result. The partial frame is already destroyed here.
        try {
            errors << "====================================================================" << std::endl;
            errors << error.toString() << std::endl;
            errors << "====================================================================" << std::endl;
        } catch (...) {
        }
        return false;
    }
}

} // namespace de
