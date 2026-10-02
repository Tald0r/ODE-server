#include "ServerFatalHandlers.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>

#include <string_view>

namespace de {
namespace {

constexpr std::string_view memoryError = "CRITICAL ERROR! NOT ENOUGH MEMORY!\n";
constexpr std::string_view unhandledException = "UNHANDLED EXCEPTION OCCURED\n";
constexpr std::string_view separator =
    "==============================================================================\n";

void writeDiagnostic(int descriptor, std::string_view message) noexcept {
    while (!message.empty()) {
        const auto written = ::write(descriptor, message.data(), message.size());
        if (written > 0)
            message.remove_prefix(static_cast<std::size_t>(written));
        else if (written < 0 && errno == EINTR)
            continue;
        else
            break;
    }
}

[[noreturn]] void abortGame(std::string_view message) noexcept {
    writeDiagnostic(STDERR_FILENO, separator);
    writeDiagnostic(STDERR_FILENO, message);
    writeDiagnostic(STDERR_FILENO, separator);

    // The normal logger allocates for timestamps and streams. A fatal memory
    // handler must not re-enter itself while trying to record its diagnostic.
    const int log = ::open("CriticalError.log", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0666);
    if (log >= 0) {
        writeDiagnostic(log, message);
        ::close(log);
    }
    std::abort();
}

[[noreturn]] void gameMemoryError() noexcept {
    abortGame(memoryError);
}

[[noreturn]] void gameUnhandledException() noexcept {
    abortGame(unhandledException);
}

[[noreturn]] void serviceMemoryError() noexcept {
    writeDiagnostic(STDERR_FILENO, memoryError);
    // The legacy process graph has no safe destruction order, and an
    // allocation failure may happen on any worker while others still run.
    std::_Exit(EXIT_FAILURE);
}

} // namespace

ServerFatalHandlers::ServerFatalHandlers(ServerKind server) noexcept
    : m_PreviousNew(std::set_new_handler(server == ServerKind::Game ? gameMemoryError : serviceMemoryError)),
      m_PreviousTerminate(server == ServerKind::Game ? std::set_terminate(gameUnhandledException) : nullptr),
      m_RestoreTerminate(server == ServerKind::Game) {}

ServerFatalHandlers::~ServerFatalHandlers() {
    if (m_RestoreTerminate)
        std::set_terminate(m_PreviousTerminate);
    std::set_new_handler(m_PreviousNew);
}

} // namespace de
