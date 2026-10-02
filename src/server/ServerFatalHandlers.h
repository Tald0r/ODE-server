#ifndef DARKEDEN_SERVER_FATAL_HANDLERS_H
#define DARKEDEN_SERVER_FATAL_HANDLERS_H

#include <exception>
#include <new>

#include "ServerKind.h"

namespace de {

// Install on the main thread before server initialization. Fatal diagnostics
// use fixed messages and file descriptors, without C++ heap allocation.
// Game allocation/termination failures append CriticalError.log and abort;
// login/shared allocation failures exit with failure without running teardown.
// Login/shared keep their existing terminate handler. Scope exit restores the
// previous handlers; the normal server path keeps them until _Exit.
class ServerFatalHandlers {
public:
    explicit ServerFatalHandlers(ServerKind server) noexcept;
    ~ServerFatalHandlers();

    ServerFatalHandlers(const ServerFatalHandlers&) = delete;
    ServerFatalHandlers& operator=(const ServerFatalHandlers&) = delete;

private:
    std::new_handler m_PreviousNew;
    std::terminate_handler m_PreviousTerminate;
    bool m_RestoreTerminate;
};

} // namespace de

#endif
