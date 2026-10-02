#include "ManagedThread.h"

void ManagedThread::start() {
    std::lock_guard lock(m_Lifecycle);
    if (getStatus() != READY || m_JoinRequested)
        throw ThreadException("invalid thread status");
    m_Worker.start([this](std::stop_token token) {
        // Creation may schedule the worker before start() returns. Wait until
        // the creator has published its native identity and running state.
        {
            std::lock_guard started(m_Lifecycle);
            m_Token = token; // Read only by this worker in run()/pauseFor().
        }
        try {
            if (!stopRequested())
                run();
            if (!stopRequested())
                throw std::runtime_error("managed worker exited unexpectedly");
        } catch (...) {
            ServerShutdown::fail();
            std::lock_guard finished(m_Lifecycle);
            setStatus(EXIT);
            throw; // CooperativeThread retains the failure for the owner.
        }
        std::lock_guard finished(m_Lifecycle);
        setStatus(EXIT);
    });
    setTID(m_Worker.nativeHandle());
    setStatus(RUNNING);
}

void ManagedThread::stop() {
    std::lock_guard lock(m_Lifecycle);
    m_Worker.requestStop();
    if (getStatus() != EXIT)
        setStatus(m_Worker.joinable() ? EXITING : EXIT);
}

void ManagedThread::join() {
    // Serializes against start while still allowing stop during a join.
    {
        std::lock_guard lock(m_Lifecycle);
        m_JoinRequested = true;
    }
    m_Worker.join();
    setStatus(EXIT);
}

void ManagedThread::rethrowFailure() const {
    m_Worker.rethrowFailure();
}

bool ManagedThread::stopRequested() const noexcept {
    return m_Token.stop_requested() || ServerShutdown::isRequested();
}

bool ManagedThread::pauseFor(std::chrono::microseconds delay) {
    std::unique_lock lock(m_WaitMutex);
    m_Wake.wait_for(lock, m_Token, delay, [] { return ServerShutdown::isRequested(); });
    return !stopRequested();
}
