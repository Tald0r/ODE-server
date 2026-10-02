#ifndef DARKEDEN_THREAD_POOL_H
#define DARKEDEN_THREAD_POOL_H

#include <exception>
#include <list>
#include <memory>
#include <mutex>

class ManagedThread;

// Owns managed workers from registration through drain. Register ready workers
// before start/stop; either lifecycle operation closes registration. Operations
// are serialized, so worker lifecycle overrides must not reenter this pool.
// Destruction requires quiescent callers on a thread outside the owned workers.
class ThreadPool {
public:
    ThreadPool();
    ~ThreadPool() noexcept;
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Consumes the incoming owner, including on refused/failed registration.
    void addThread(std::unique_ptr<ManagedThread> thread);
    void start();
    // Attempts every stop before every join, then reports retained failures.
    // Operational/diagnostic errors propagate after the drain attempt. Once
    // joined, workers are not drained or reported again on repeated stop.
    void stop();

private:
    std::exception_ptr drain() noexcept; // Caller holds m_Mutex.
    std::mutex m_Mutex;
    std::list<std::unique_ptr<ManagedThread>> m_Threads;
    bool m_Closed = false;
    bool m_Drained = false;
};

#endif
