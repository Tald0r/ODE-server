#include "ServerPortSettings.h"

#include <charconv>
#include <limits>

#include <string_view>
#include <system_error>

#include "Exception.h"
#include "Properties.h"

namespace de {

std::uint16_t readServerPort(const Properties& config, const std::string& key) {
    const std::string value = config.getProperty(key);
    std::string_view text(value);
    const auto first = text.find_first_not_of(" \t");
    if (first != std::string_view::npos) {
        text.remove_prefix(first);
        const auto last = text.find_last_not_of(" \t\r");
        text = last == std::string_view::npos ? std::string_view{} : text.substr(0, last + 1);
    } else {
        text = {};
    }
    if (!text.empty() && text.front() == '+')
        text.remove_prefix(1);

    const auto invalid = [&] { return Error(key + " must be a decimal port from 1 to 65535"); };
    if (text.empty())
        throw invalid();
    unsigned int port = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || port == 0 ||
        port > std::numeric_limits<std::uint16_t>::max())
        throw invalid();
    return static_cast<std::uint16_t>(port);
}

void validateServerListenerPorts(ServerKind server, const Properties& config) {
    switch (server) {
    case ServerKind::Game:
        (void)readServerPort(config, "TCPPort");
        (void)readServerPort(config, "GameServerUDPPort");
        return;
    case ServerKind::Login:
        (void)readServerPort(config, "LoginServerPort");
        (void)readServerPort(config, "LoginServerUDPPort");
        return;
    case ServerKind::Shared:
        (void)readServerPort(config, "TCPPort");
        return;
    }
    throw Error("Unknown server kind");
}

} // namespace de
