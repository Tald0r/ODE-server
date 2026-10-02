#ifndef DARKEDEN_SERVER_START_SEQUENCE_H
#define DARKEDEN_SERVER_START_SEQUENCE_H

#include <functional>
#include <span>

namespace de {
using ServerStartAction = std::function<void()>;

// Run background startup steps in order, then the blocking main loop. Check
// process shutdown before each action; an action already underway must still
// cooperate with shutdown itself. Exceptions propagate without translation.
// Does not reset shutdown state or drain workers: the server lifecycle owns
// cleanup, including partially started groups.
void runServerStartSequence(std::span<const ServerStartAction> backgroundStarts, const ServerStartAction& runMainLoop);
} // namespace de

#endif
