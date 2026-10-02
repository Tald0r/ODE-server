#ifndef DARKEDEN_SERVER_STARTUP_H
#define DARKEDEN_SERVER_STARTUP_H

#include <memory>
#include <optional>
#include <string>

#include "Properties.h"

namespace de {

enum class ServerKind { Game, Login, Shared };

struct ServerOptions {
    std::string configFile;
    std::optional<int> loginIDOffset;
};

// argv includes the executable name. Accept -f <file> for every server and
// an optional -i <signed decimal offset> for loginserver. Invalid arguments
// throw Error with the usage for that server; no process state is changed.
ServerOptions parseServerOptions(ServerKind server, int argc, const char* const argv[]);

// Compute all three overrides before changing any property. Missing or invalid
// bases and an overflowing sum leave the configuration unchanged.
void applyLoginServerOffset(Properties& config, int offset);

// The caller owns the completed configuration and decides when to publish it
// to KernelContext. File/parse/override failures propagate before publication.
std::unique_ptr<Properties> loadServerConfiguration(const ServerOptions& options);

} // namespace de

#endif
