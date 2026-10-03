#ifndef DARKEDEN_LOGIN_DATAGRAM_SEND_H
#define DARKEDEN_LOGIN_DATAGRAM_SEND_H

#include <functional>
#include <iosfwd>
#include <string>

#include "Types.h"

class Datagram;
class DatagramPacket;

namespace de {

using LoginDatagramTransport = std::function<uint(Datagram&)>;

// Serialize and send one frame using a supplied synchronous transport. The
// transport borrows the datagram without mutation only until it returns.
// Require a canonical decimal IPv4 address, port 1..65535 and an exact frame byte count.
// Throwable failures return false after best-effort reporting, including endpoint
// and serialization errors. Other exception types retain their identity.
[[nodiscard]] bool sendLoginDatagram(const std::string& host, uint port, const DatagramPacket& packet,
                                     const LoginDatagramTransport& transport, std::ostream& errors);

} // namespace de

#endif
