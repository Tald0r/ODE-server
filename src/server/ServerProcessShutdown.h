#ifndef DARKEDEN_SERVER_PROCESS_SHUTDOWN_H
#define DARKEDEN_SERVER_PROCESS_SHUTDOWN_H

#include <chrono>
#include <optional>

#include <string_view>

#include "ServerShutdown.h"

namespace de {

// Own the process's SIGTERM/SIGINT handlers and shutdown deadline. Construct
// before initialization so a signal also bounds blocked startup. This does
// not clear the shared shutdown flags or own any server/worker state.
class ServerProcessShutdown {
public:
    explicit ServerProcessShutdown(std::string_view process,
                                   std::chrono::milliseconds timeout = std::chrono::seconds(30));

    ServerProcessShutdown(const ServerProcessShutdown&) = delete;
    ServerProcessShutdown& operator=(const ServerProcessShutdown&) = delete;

    // False means signal installation failed; the caller must not start the
    // server. A partially installed handler is rolled back and no watcher runs.
    bool ready() const noexcept;

private:
    // A member guard also restores handlers if starting the deadline throws.
    // Normal destruction cancels and joins the watcher before restoring them.
    struct SignalHandlers {
        SignalHandlers() noexcept;
        ~SignalHandlers();

        struct sigaction previousTerm {};
        struct sigaction previousInt {};
        bool installed = false;
    };

    SignalHandlers m_Signals;
    std::optional<ServerShutdown::Deadline> m_Deadline;
};

} // namespace de

#endif
