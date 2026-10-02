#include "ThreadPool.h"

#include <iostream>
#include <utility>

#include "ManagedThread.h"

ThreadPool::ThreadPool() = default;

ThreadPool::~ThreadPool() noexcept {
    try {
        stop();
    } catch (...) {
        ServerShutdown::fail();
    }
    // A failed diagnostic must not prevent deletion, but an unjoined worker
    // must never outlive its derived state or borrowed world dependencies.
    if (!m_Drained)
        std::terminate();
}

void ThreadPool::addThread(std::unique_ptr<ManagedThread> thread) {
    std::lock_guard lock(m_Mutex);
    if (!thread)
        throw Error("cannot register a null worker");
    if (m_Closed || thread->getStatus() != Thread::READY)
        throw Error("worker registration requires a ready worker and an unstarted pool");
    m_Threads.push_back(std::move(thread));
}

void ThreadPool::start() {
    std::lock_guard lock(m_Mutex);
    if (m_Closed)
        throw Error("thread pool is already started or stopped");
    m_Closed = true;
    try {
        for (const auto& thread : m_Threads)
            thread->start();
    } catch (...) {
        const auto failure = std::current_exception();
        // Arm the process deadline before rollback can wait on blocked work.
        ServerShutdown::fail();
        (void)drain();
        std::rethrow_exception(failure);
    }
}

void ThreadPool::stop() {
    std::lock_guard lock(m_Mutex);
    m_Closed = true;
    if (m_Drained)
        return;
    if (auto failure = drain())
        std::rethrow_exception(failure);
}

std::exception_ptr ThreadPool::drain() noexcept {
    std::exception_ptr failure;
    const auto recordFailure = [&] {
        ServerShutdown::fail();
        if (!failure)
            failure = std::current_exception();
    };
    for (const auto& thread : m_Threads) {
        try {
            thread->stop();
        } catch (...) {
            recordFailure();
            // An override may fail before it requests cooperative cancellation.
            try {
                thread->ManagedThread::stop();
            } catch (...) {
                recordFailure();
            }
        }
    }
    bool joined = true;
    for (const auto& thread : m_Threads) {
        try {
            thread->join();
        } catch (...) {
            recordFailure();
            try {
                thread->ManagedThread::join();
            } catch (...) {
                joined = false;
                recordFailure();
            }
        }
    }
    m_Drained = joined;
    for (const auto& thread : m_Threads) {
        try {
            try {
                thread->rethrowFailure();
            } catch (const Throwable& error) {
                std::cerr << thread->getName() << ": " << error.toString() << std::endl;
            } catch (const std::exception& error) {
                std::cerr << thread->getName() << ": " << error.what() << std::endl;
            } catch (...) {
                std::cerr << thread->getName() << ": unknown worker failure" << std::endl;
            }
        } catch (...) {
            recordFailure();
        }
    }
    return failure;
}
