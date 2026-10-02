#ifndef DARKEDEN_MANAGED_THREAD_H
#define DARKEDEN_MANAGED_THREAD_H

#include <mutex>

#include <condition_variable>

#include "CooperativeThread.h"
#include "ServerShutdown.h"
#include "Thread.h"

// The owning manager must join before destroying dependencies; each derived
// destructor must stop/join BEFORE destroying its own members. A base
// destructor is too late. The worker body starts after identity publication.
class ManagedThread : public Thread {
public:
    void start() override;
    void stop() override;
    void join() override;

    // No detach: a worker cannot be separated from the object whose state it uses.
    void rethrowFailure() const;

protected:
    bool stopRequested() const noexcept;
    bool pauseFor(std::chrono::microseconds delay);

private:
    std::mutex m_Lifecycle;
    bool m_JoinRequested = false;
    std::mutex m_WaitMutex;
    std::condition_variable_any m_Wake;
    std::stop_token m_Token;
    CooperativeThread m_Worker;
};

#endif
