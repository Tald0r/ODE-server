#ifndef DARKEDEN_DIAGNOSTIC_TRACE_H
#define DARKEDEN_DIAGNOSTIC_TRACE_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>

namespace de {

inline std::mutex& diagnosticTraceMutex() {
    static std::mutex mutex;
    return mutex;
}

// Set before starting the process. Disabled diagnostics neither construct their
// message nor evaluate the supplied writer. Failures must not affect gameplay.
template <typename Writer> void diagnosticTrace(Writer&& write) noexcept {
    const char* enabled = std::getenv("DARKEDEN_TRACE");
    if (!enabled || std::strcmp(enabled, "1") != 0)
        return;
    try {
        std::ostringstream message;
        message << "[debug] ";
        write(message);
        message << '\n';
        if (!message.good())
            return;
        const std::string text = message.str();
        std::lock_guard lock(diagnosticTraceMutex());
        // A failed optional trace must not poison cerr's state and suppress
        // later error messages. Write through its buffer without changing
        // the shared stream's state or exception mask.
        std::streambuf* destination = std::cerr.rdbuf();
        if (!destination)
            return;
        const auto size = static_cast<std::streamsize>(text.size());
        if (destination->sputn(text.data(), size) != size)
            return;
        (void)destination->pubsync();
    } catch (...) {
    }
}

} // namespace de

#endif
