#ifndef DARKEDEN_SERVER_LIFECYCLE_H
#define DARKEDEN_SERVER_LIFECYCLE_H

#include <functional>
#include <iosfwd>
#include <string>

namespace de {

struct ServerLifecycleActions {
    // Construct and initialize the server. Keep any completed construction
    // reachable by stop even when initialization throws.
    std::function<void()> initialize;
    std::function<void()> start;
    // Must tolerate failed initialization, including no constructed server.
    std::function<void()> stop;
};

struct ServerLifecycleResult {
    bool drained;
    int exitCode;
};

// Run once on the main thread. Initialize, start unless shutdown is already
// requested, then request shutdown and attempt stop even after a failure.
// Uses the same ServerShutdown state as workers; never clears a request or
// failure. Reports exceptions before continuing teardown and returns its
// outcome without destroying the server, installing signals or exiting.
[[nodiscard]] ServerLifecycleResult runServerLifecycle(const ServerLifecycleActions& actions, std::ostream& output,
                                                       std::ostream& errors,
                                                       const std::string& instantLogPath = "../log/instant.log");

} // namespace de

#endif
