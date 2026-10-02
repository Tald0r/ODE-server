#include "ServerWorkerShutdown.h"

#include <exception>
#include <ostream>

#include "ManagedThread.h"

namespace de {

void stopServerWorkers(std::span<const ServerWorker> workers, std::ostream& errors,
                       const std::function<void()>& beforeJoin) {
    for (const auto& worker : workers)
        worker.thread.stop();

    if (beforeJoin)
        beforeJoin();

    for (const auto& worker : workers) {
        worker.thread.join();
        const auto report = [&](const auto& message) {
            if (worker.failureName.empty())
                errors << worker.thread.getName();
            else
                errors << worker.failureName;
            errors << ": " << message << std::endl;
        };
        try {
            worker.thread.rethrowFailure();
        } catch (const Throwable& error) {
            report(error.toString());
        } catch (const std::exception& error) {
            report(error.what());
        } catch (...) {
            report("unknown worker failure");
        }
    }
}

} // namespace de
