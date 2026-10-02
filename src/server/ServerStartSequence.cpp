#include "ServerStartSequence.h"

#include "ServerShutdown.h"

namespace de {
void runServerStartSequence(std::span<const ServerStartAction> backgroundStarts, const ServerStartAction& runMainLoop) {
    for (const auto& start : backgroundStarts) {
        if (ServerShutdown::isRequested())
            return;
        start();
    }
    if (!ServerShutdown::isRequested())
        runMainLoop();
}
} // namespace de
