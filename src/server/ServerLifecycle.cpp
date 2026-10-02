#include "ServerLifecycle.h"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <ostream>

#include "Exception.h"
#include "ServerShutdown.h"

namespace de {

ServerLifecycleResult runServerLifecycle(const ServerLifecycleActions& actions, std::ostream& output,
                                         std::ostream& errors, const std::string& instantLogPath) {
    try {
        actions.initialize();
        if (!ServerShutdown::isRequested())
            actions.start();
    } catch (const Throwable& error) {
        // Startup can fail before the normal logging infrastructure is ready.
        std::ofstream instantLog(instantLogPath, std::ios::out);
        instantLog << error.toString() << std::endl;
        instantLog.close();
        output << error.toString() << std::endl;
        ServerShutdown::fail();
    } catch (...) {
        output << "unknown exception..." << std::endl;
        ServerShutdown::fail();
    }

    // Stop while the server's configuration and managers are still alive.
    ServerShutdown::request();
    bool drained = true;
    try {
        actions.stop();
    } catch (const Throwable& error) {
        drained = false;
        ServerShutdown::fail();
        errors << "Shutdown failed: " << error.toString() << std::endl;
    } catch (const std::exception& error) {
        drained = false;
        ServerShutdown::fail();
        errors << "Shutdown failed: " << error.what() << std::endl;
    } catch (...) {
        drained = false;
        ServerShutdown::fail();
        errors << "Shutdown failed: unknown exception" << std::endl;
    }
    return {drained, ServerShutdown::failed.load() ? EXIT_FAILURE : EXIT_SUCCESS};
}

} // namespace de
