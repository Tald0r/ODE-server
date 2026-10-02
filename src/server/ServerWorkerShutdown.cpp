#include "ServerWorkerShutdown.h"

#include <exception>
#include <ostream>

#include "ManagedThread.h"

namespace de {

void stopServerWorkers(std::span<const ServerWorker> workers, std::ostream& errors,
                       const std::function<void()>& beforeJoin) {
    std::exception_ptr failure;
    const auto recordFailure = [&] {
        // Request process shutdown before a later drain operation can block.
        ServerShutdown::fail();
        if (!failure)
            failure = std::current_exception();
    };
    for (const auto& worker : workers) {
        try {
            worker.thread.stop();
        } catch (...) {
            recordFailure();
            // An override can throw before requesting cooperative cancellation.
            try {
                worker.thread.ManagedThread::stop();
            } catch (...) {
                recordFailure();
            }
        }
    }

    try {
        if (beforeJoin)
            beforeJoin();
    } catch (...) {
        recordFailure();
    }

    for (const auto& worker : workers) {
        try {
            worker.thread.join();
        } catch (...) {
            recordFailure();
            try {
                worker.thread.ManagedThread::join();
            } catch (...) {
                recordFailure();
            }
        }
    }

    // Reporting must not stand between a worker and the remaining joins.
    for (const auto& worker : workers) {
        const auto report = [&](const auto& message) {
            if (worker.failureName.empty())
                errors << worker.thread.getName();
            else
                errors << worker.failureName;
            errors << ": " << message << std::endl;
        };
        try {
            try {
                worker.thread.rethrowFailure();
            } catch (const Throwable& error) {
                report(error.toString());
            } catch (const std::exception& error) {
                report(error.what());
            } catch (...) {
                report("unknown worker failure");
            }
        } catch (...) {
            recordFailure();
        }
    }
    if (failure)
        std::rethrow_exception(failure);
}

} // namespace de
