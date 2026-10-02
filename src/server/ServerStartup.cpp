#include "ServerStartup.h"

#include <charconv>
#include <limits>

#include <string_view>
#include <system_error>

#include "Exception.h"

namespace de {
namespace {

const char* usage(ServerKind server) {
    switch (server) {
    case ServerKind::Game:
        return "Usage : gameserver -f <config file>";
    case ServerKind::Login:
        return "Usage : loginserver -f <config file> [-i ID]";
    case ServerKind::Shared:
        return "Usage : sharedserver -f <config file>";
    }
    throw Error("Unknown server kind");
}

int decimalInteger(std::string_view text, const std::string& error) {
    // from_chars accepts a minus but not a plus; both were valid with atoi.
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
        if (text.empty() || text.front() == '-')
            throw Error(error);
    }
    if (text.empty())
        throw Error(error);

    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw Error(error);
    return value;
}

std::string offsetProperty(const Properties& config, const char* base, int offset) {
    const int value = decimalInteger(config.getProperty(base), std::string(base) + " must be a decimal integer");
    const long long sum = static_cast<long long>(value) + offset;
    if (sum < std::numeric_limits<int>::min() || sum > std::numeric_limits<int>::max())
        throw Error(std::string(base) + " plus -i offset is outside the integer range");
    return std::to_string(sum);
}

} // namespace

ServerOptions parseServerOptions(ServerKind server, int argc, const char* const argv[]) {
    const std::string help = usage(server);
    if ((argc != 3 && !(server == ServerKind::Login && argc == 5)) || argv == nullptr)
        throw Error(help);
    if (argv[1] == nullptr || std::string_view(argv[1]) != "-f" || argv[2] == nullptr || argv[2][0] == '\0')
        throw Error(help);

    ServerOptions options;
    options.configFile = argv[2];
    if (argc == 5) {
        if (argv[3] == nullptr || std::string_view(argv[3]) != "-i" || argv[4] == nullptr)
            throw Error(help);
        options.loginIDOffset = decimalInteger(argv[4], "Invalid loginserver -i offset. " + help);
    }
    return options;
}

void applyLoginServerOffset(Properties& config, int offset) {
    const std::string port = offsetProperty(config, "LoginServerBasePort", offset);
    const std::string udpPort = offsetProperty(config, "LoginServerBaseUDPPort", offset);
    const std::string id = offsetProperty(config, "LoginServerBaseID", offset);
    config.setProperty("LoginServerPort", port);
    config.setProperty("LoginServerUDPPort", udpPort);
    config.setProperty("LoginServerID", id);
}

std::unique_ptr<Properties> loadServerConfiguration(const ServerOptions& options) {
    auto config = std::make_unique<Properties>();
    config->load(options.configFile);
    if (options.loginIDOffset)
        applyLoginServerOffset(*config, *options.loginIDOffset);
    return config;
}

} // namespace de
