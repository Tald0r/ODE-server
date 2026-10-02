#include "ServerProcessShutdown.h"

namespace de {

ServerProcessShutdown::SignalHandlers::SignalHandlers() noexcept {
    struct sigaction action {};
    action.sa_handler = ServerShutdown::request;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGTERM, &action, &previousTerm) != 0)
        return;
    if (sigaction(SIGINT, &action, &previousInt) != 0) {
        sigaction(SIGTERM, &previousTerm, nullptr);
        return;
    }
    installed = true;
}

ServerProcessShutdown::SignalHandlers::~SignalHandlers() {
    if (installed) {
        sigaction(SIGINT, &previousInt, nullptr);
        sigaction(SIGTERM, &previousTerm, nullptr);
    }
}

ServerProcessShutdown::ServerProcessShutdown(std::string_view process, std::chrono::milliseconds timeout) {
    if (m_Signals.installed)
        m_Deadline.emplace(timeout, process);
}

bool ServerProcessShutdown::ready() const noexcept {
    return m_Signals.installed;
}

} // namespace de
