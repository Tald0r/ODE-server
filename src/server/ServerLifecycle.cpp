#include "ServerLifecycle.h"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <ostream>

#include "Exception.h"
#include "ServerShutdown.h"

namespace de {
namespace {

// Failure reporting is best effort: a formatter, file or stream must not
// bypass cleanup or replace the lifecycle's drain result.
template <typename Report> void reportFailure(Report&& report) noexcept {
    try {
        report();
    } catch (...) {
        ServerShutdown::fail();
    }
}

} // namespace

ServerLifecycleResult runServerLifecycle(const ServerLifecycleActions& actions, std::ostream& output,
                                         std::ostream& errors, const std::string& instantLogPath) {
    try {
        actions.initialize();
        if (!ServerShutdown::isRequested())
            actions.start();
    } catch (const Throwable& error) {
        // Arm the process deadline before diagnostics can fail or block.
        ServerShutdown::fail();
        // Startup can fail before the normal logging infrastructure is ready.
        // Attempt each destination independently, even if the first throws.
        reportFailure([&] {
            std::ofstream instantLog(instantLogPath, std::ios::out);
            instantLog << error.toString() << std::endl;
            instantLog.close();
        });
        reportFailure([&] { output << error.toString() << std::endl; });
    } catch (...) {
        ServerShutdown::fail();
        reportFailure([&] { output << "unknown exception..." << std::endl; });
    }

    // Stop while the server's configuration and managers are still alive.
    ServerShutdown::request();
    bool drained = true;
    try {
        actions.stop();
    } catch (const Throwable& error) {
        drained = false;
        ServerShutdown::fail();
        reportFailure([&] { errors << "Shutdown failed: " << error.toString() << std::endl; });
    } catch (const std::exception& error) {
        drained = false;
        ServerShutdown::fail();
        reportFailure([&] { errors << "Shutdown failed: " << error.what() << std::endl; });
    } catch (...) {
        drained = false;
        ServerShutdown::fail();
        reportFailure([&] { errors << "Shutdown failed: unknown exception" << std::endl; });
    }
    return {drained, ServerShutdown::failed.load() ? EXIT_FAILURE : EXIT_SUCCESS};
}

} // namespace de
