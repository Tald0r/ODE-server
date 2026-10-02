#ifndef DARKEDEN_SERVER_WORKER_SHUTDOWN_H
#define DARKEDEN_SERVER_WORKER_SHUTDOWN_H

#include <functional>
#include <iosfwd>
#include <span>

#include <string_view>

class ManagedThread;

namespace de {

// A borrowed worker, with an optional diagnostic name for managers whose
// Thread::getName() still returns the generic "Thread" label.
struct ServerWorker {
    ManagedThread& thread;
    std::string_view failureName = {};
};

// Request every worker's stop before joining any of them. The optional action
// drains other workers (the game's zone pool) after these stop requests and
// before these joins. Keep all workers and their dependencies alive throughout.
// Retained run failures are reported without interrupting the remaining joins;
// stop/action/join errors propagate to the server lifecycle's failure handling.
// Does not own workers, clear shutdown flags or impose a shutdown deadline.
void stopServerWorkers(std::span<const ServerWorker> workers, std::ostream& errors,
                       const std::function<void()>& beforeJoin = {});

} // namespace de

#endif
