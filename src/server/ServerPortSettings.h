#ifndef DARKEDEN_SERVER_PORT_SETTINGS_H
#define DARKEDEN_SERVER_PORT_SETTINGS_H

#include <cstdint>
#include <string>

#include "ServerKind.h"

class Properties;

namespace de {

// Read a required, complete decimal port in 1..65535. A leading plus, leading
// zeroes, surrounding spaces/tabs and a CRLF file's trailing CR are accepted.
// Missing properties throw NoSuchElementException; invalid values throw Error
// naming the property. No configuration or process state is changed.
std::uint16_t readServerPort(const Properties& config, const std::string& key);

// Validate the effective ports after login offsets and before publication or
// manager construction. Game requires TCPPort/GameServerUDPPort, login requires
// LoginServerPort/LoginServerUDPPort, and shared requires TCPPort. Other settings
// (including optional proxy and outbound ports) remain with their own readers.
void validateServerListenerPorts(ServerKind server, const Properties& config);

} // namespace de

#endif
