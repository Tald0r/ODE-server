#include <unistd.h>

#include <barrier>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "ManagedThread.h"
#include "ThreadPool.h"
#include "support/AllocationProbe.h"

namespace {
class CountedWorker : public ManagedThread {
public:
    explicit CountedWorker(unsigned& destroyed) : destroyed(destroyed) {}
    ~CountedWorker() noexcept override {
        stop();
        join();
        ++destroyed;
    }
    void run() override {
        while (pauseFor(std::chrono::milliseconds(1))) {
        }
    }

private:
    unsigned& destroyed;
};

class FailingDiagnostic : public Error {
public:
    std::string toString() const override {
        throw std::bad_alloc();
    }
};

class DiagnosticWorker : public CountedWorker {
public:
    DiagnosticWorker(unsigned& destroyed, std::promise<void>& entered) : CountedWorker(destroyed), entered(entered) {}
    void run() override {
        entered.set_value();
        throw FailingDiagnostic();
    }

private:
    std::promise<void>& entered;
};

TEST(WorkerPoolOwnership, FailedRegistrationReleasesTheIncomingWorker) {
    ASSERT_EXIT(
        {
            unsigned destroyed = 0;
            {
                ThreadPool pool;
                auto worker = std::make_unique<CountedWorker>(destroyed);
                AllocationProbe probe(1);
                bool threw = false;
                try {
                    pool.addThread(std::move(worker));
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                probe.stopFailing();
                if (!threw || !probe.rejected() || probe.outstanding() != 0)
                    std::_Exit(1);
            }
            std::_Exit(destroyed == 1 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(WorkerPoolOwnership, DestructionReleasesWorkersEvenWhenFailureDiagnosticsThrow) {
    ASSERT_EXIT(
        {
            ::alarm(3);
            ServerShutdown::requested = false;
            ServerShutdown::failed = false;
            unsigned destroyed = 0;
            std::promise<void> entered;
            auto ready = entered.get_future();
            auto pool = std::make_unique<ThreadPool>();
            pool->addThread(std::make_unique<DiagnosticWorker>(destroyed, entered));
            pool->start();
            ready.wait();
            pool.reset();
            std::_Exit(destroyed == 1 && ServerShutdown::failed.load() ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

int checkRegistrationFailure(std::size_t failAt) {
    unsigned firstDestroyed = 0;
    unsigned incomingDestroyed = 0;
    auto pool = std::make_unique<ThreadPool>();
    pool->addThread(std::make_unique<CountedWorker>(firstDestroyed));
    auto incoming = std::make_unique<CountedWorker>(incomingDestroyed);
    AllocationProbe probe(failAt);
    bool threw = false;
    try {
        pool->addThread(std::move(incoming));
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    probe.stopFailing();
    if (threw != probe.rejected() || (failAt == 4 && probe.rejected()) || incoming || firstDestroyed != 0 ||
        incomingDestroyed != (threw ? 1u : 0u))
        return 1;
    if (threw) {
        if (probe.outstanding() != 0)
            return 2;
        pool->addThread(std::make_unique<CountedWorker>(incomingDestroyed));
    }
    // These workers never started; the allocation probe remains single-threaded.
    pool.reset();
    return firstDestroyed == 1 && incomingDestroyed == (threw ? 2u : 1u) && probe.outstanding() == 0 ? 0 : 3;
}

TEST(WorkerPoolOwnership, AllocationFailuresPreserveExistingOwnersAndPermitRegistrationRetry) {
    for (std::size_t failAt = 1; failAt <= 4; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkRegistrationFailure(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

class WorkerPoolTest : public testing::Test {
    void SetUp() override {
        ServerShutdown::requested = false;
        ServerShutdown::failed = false;
    }
    void TearDown() override {
        ServerShutdown::requested = false;
        ServerShutdown::failed = false;
    }
};

class StartupFailure : public std::runtime_error {
public:
    StartupFailure() : std::runtime_error("startup failure") {}
};
class StopFailure : public std::runtime_error {
public:
    StopFailure() : std::runtime_error("stop failure") {}
};
class JoinFailure : public std::runtime_error {
public:
    JoinFailure() : std::runtime_error("join failure") {}
};

struct Observation {
    std::promise<void> entered;
    unsigned destroyed = 0;
    bool failStart = false;
    bool failStop = false;
    bool failJoin = false;
};

class ObservedWorker : public ManagedThread {
public:
    ObservedWorker(unsigned id, Observation& observation, std::vector<unsigned>& events)
        : id(id), observation(observation), events(events) {}
    ~ObservedWorker() noexcept override {
        ManagedThread::stop();
        ManagedThread::join();
        ++observation.destroyed;
    }
    void start() override {
        events.push_back(100 + id);
        if (observation.failStart)
            throw StartupFailure();
        ManagedThread::start();
    }
    void stop() override {
        events.push_back(200 + id);
        if (observation.failStop)
            throw StopFailure();
        ManagedThread::stop();
    }
    void join() override {
        events.push_back(300 + id);
        if (observation.failJoin)
            throw JoinFailure();
        ManagedThread::join();
    }
    void run() override {
        observation.entered.set_value();
        while (pauseFor(std::chrono::seconds(5))) {
        }
    }

private:
    unsigned id;
    Observation& observation;
    std::vector<unsigned>& events;
};

TEST_F(WorkerPoolTest, StartupRollbackDrainsEveryOwnerAndPreservesTheOriginalFailure) {
    Observation observations[3];
    observations[0].failStop = true;
    observations[0].failJoin = true;
    observations[1].failStart = true;
    std::vector<unsigned> events;
    events.reserve(32);
    {
        ThreadPool pool;
        ManagedThread* borrowed[3]{};
        for (unsigned id = 0; id < 3; ++id) {
            auto worker = std::make_unique<ObservedWorker>(id, observations[id], events);
            borrowed[id] = worker.get();
            pool.addThread(std::move(worker));
        }
        EXPECT_THROW(pool.start(), StartupFailure);
        const std::vector<unsigned> expected = {100, 101, 200, 201, 202, 300, 301, 302};
        EXPECT_EQ(events, expected);
        for (const auto* worker : borrowed)
            EXPECT_EQ(worker->getStatus(), Thread::EXIT);
        EXPECT_TRUE(ServerShutdown::failed.load());
        EXPECT_NO_THROW(pool.stop());
        EXPECT_THROW(pool.start(), Error);
        EXPECT_EQ(events, expected);
    }
    for (const auto& observation : observations)
        EXPECT_EQ(observation.destroyed, 1u);
}

TEST_F(WorkerPoolTest, StopAndJoinFailuresDoNotSkipOtherWorkersOrRepeatACompletedDrain) {
    Observation observations[3];
    observations[0].failStop = true;
    observations[1].failJoin = true;
    std::vector<unsigned> events;
    events.reserve(32);
    {
        ThreadPool pool;
        std::future<void> entered[3];
        for (unsigned id = 0; id < 3; ++id) {
            entered[id] = observations[id].entered.get_future();
            pool.addThread(std::make_unique<ObservedWorker>(id, observations[id], events));
        }
        pool.start();
        for (auto& ready : entered)
            ready.wait();
        EXPECT_THROW(pool.stop(), StopFailure);
        const std::vector<unsigned> expected = {100, 101, 102, 200, 201, 202, 300, 301, 302};
        EXPECT_EQ(events, expected);
        EXPECT_TRUE(ServerShutdown::failed.load());
        EXPECT_NO_THROW(pool.stop());
        EXPECT_EQ(events, expected);
    }
    for (const auto& observation : observations)
        EXPECT_EQ(observation.destroyed, 1u);
}

TEST_F(WorkerPoolTest, NullNonReadyAndLateRegistrationAreRefusedWithoutAbandoningOwnership) {
    unsigned destroyed = 0;
    {
        ThreadPool pool;
        EXPECT_THROW(pool.addThread({}), Error);
        auto stopped = std::make_unique<CountedWorker>(destroyed);
        stopped->stop();
        EXPECT_THROW(pool.addThread(std::move(stopped)), Error);
        EXPECT_EQ(destroyed, 1u);
        pool.addThread(std::make_unique<CountedWorker>(destroyed));
        pool.start();
        EXPECT_THROW(pool.addThread(std::make_unique<CountedWorker>(destroyed)), Error);
        EXPECT_EQ(destroyed, 2u);
        pool.stop();
        EXPECT_THROW(pool.addThread(std::make_unique<CountedWorker>(destroyed)), Error);
        EXPECT_EQ(destroyed, 3u);
    }
    EXPECT_EQ(destroyed, 4u);
}

class ObservedDiagnostic : public Error {
public:
    ObservedDiagnostic(unsigned& attempts, bool fail) : attempts(attempts), fail(fail) {}
    std::string toString() const override {
        ++attempts;
        if (fail)
            throw std::bad_alloc();
        return "second worker failure";
    }

private:
    unsigned& attempts;
    bool fail;
};

class ReportingWorker : public CountedWorker {
public:
    ReportingWorker(unsigned& destroyed, unsigned& attempts, bool fail, std::barrier<>& ready)
        : CountedWorker(destroyed), attempts(attempts), fail(fail), ready(ready) {}
    void run() override {
        ready.arrive_and_wait();
        throw ObservedDiagnostic(attempts, fail);
    }

private:
    unsigned& attempts;
    bool fail;
    std::barrier<>& ready;
};

TEST_F(WorkerPoolTest, DiagnosticFailureDoesNotSkipOtherReportsOrReplayThemOnDestruction) {
    unsigned destroyed = 0;
    unsigned attempts[2]{};
    std::barrier ready(3);
    {
        ThreadPool pool;
        pool.addThread(std::make_unique<ReportingWorker>(destroyed, attempts[0], true, ready));
        pool.addThread(std::make_unique<ReportingWorker>(destroyed, attempts[1], false, ready));
        pool.start();
        ready.arrive_and_wait();
        EXPECT_THROW(pool.stop(), std::bad_alloc);
        EXPECT_EQ(attempts[0], 1u);
        EXPECT_EQ(attempts[1], 1u);
        EXPECT_NO_THROW(pool.stop());
    }
    EXPECT_EQ(destroyed, 2u);
    EXPECT_EQ(attempts[0], 1u);
    EXPECT_EQ(attempts[1], 1u);
}
} // namespace
