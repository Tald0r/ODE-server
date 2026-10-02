#ifndef __THREAD_H__
#define __THREAD_H__

#include <atomic>

#include "Exception.h"
#include "Types.h"
#include "pthreadAPI.h"

// Shared identity/status interface. Production workers use ManagedThread;
// lifecycle operations have no legacy pthread creation/detachment fallback.
class Thread {
public:
    enum ThreadStatus { READY, RUNNING, EXITING, EXIT };

    Thread() = default;
    virtual ~Thread() noexcept(false) = default;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void join() = 0;
    static void join(const Thread& thread);
    virtual void run() = 0;

    static TID self();
    virtual string toString() const;
    TID getTID() const noexcept {
        return m_TID.load(std::memory_order_acquire);
    }
    ThreadStatus getStatus() const noexcept {
        return m_Status.load(std::memory_order_acquire);
    }
    void setStatus(ThreadStatus status) noexcept {
        m_Status.store(status, std::memory_order_release);
    }
    virtual string getName() const {
        return "Thread";
    }

protected:
    void setTID(TID tid) noexcept {
        m_TID.store(tid, std::memory_order_release);
    }

private:
    std::atomic<TID> m_TID{};
    std::atomic<ThreadStatus> m_Status{READY};
};

#endif
