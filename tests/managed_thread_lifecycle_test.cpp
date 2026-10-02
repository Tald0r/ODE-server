#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <future>
#include <thread>

#include <gtest/gtest.h>
#include <type_traits>

#include "ManagedThread.h"
#include "support/AllocationProbe.h"

using namespace std::chrono_literals;

namespace {
class MetadataWorker : public ManagedThread {
public:
    ~MetadataWorker() noexcept override {
        stop();
        join();
    }
    void run() override {
        const auto id = getTID();
        published.set_value(id != TID{} && ::pthread_equal(id, Thread::self()) && getStatus() == RUNNING);
        while (pauseFor(1s)) {
        }
    }
    std::promise<bool> published;
};

class ManagedLifecycleTest : public testing::Test {
    void SetUp() override {
        ServerShutdown::requested = false;
        ServerShutdown::failed = false;
    }
    void TearDown() override {
        ServerShutdown::requested = false;
        ServerShutdown::failed = false;
    }
};
} // namespace

TEST_F(ManagedLifecycleTest, RunSeesItsPublishedIdentityAndRunningState) {
    for (unsigned attempt = 0; attempt < 1024; ++attempt) {
        MetadataWorker worker;
        auto published = worker.published.get_future();
        worker.start();
        const bool observed = published.get();
        worker.stop();
        worker.join();
        ASSERT_TRUE(observed) << "worker body ran before startup publication at attempt " << attempt;
    }
}

TEST_F(ManagedLifecycleTest, MetadataCanBeReadWhileAnotherThreadStartsTheWorker) {
    MetadataWorker worker;
    auto published = worker.published.get_future();
    std::atomic<bool> finished = false;
    std::jthread starter([&] {
        worker.start();
        finished = true;
    });
    while (!finished.load()) {
        (void)worker.getTID();
        (void)worker.getStatus();
        EXPECT_FALSE(worker.toString().empty());
    }
    starter.join();
    EXPECT_TRUE(published.get());
    const auto id = worker.getTID();
    worker.stop();
    Thread::join(worker);
    EXPECT_EQ(worker.getStatus(), Thread::EXIT);
    EXPECT_EQ(worker.getTID(), id);
}

TEST_F(ManagedLifecycleTest, SelfJoinRetainsFailureAndTheOwnerCanStillDrain) {
    class SelfJoiningWorker : public ManagedThread {
    public:
        void run() override {
            join();
        }
    } worker;
    worker.start();
    worker.join();
    EXPECT_EQ(worker.getStatus(), Thread::EXIT);
    EXPECT_TRUE(ServerShutdown::failed.load());
    EXPECT_THROW(worker.rethrowFailure(), std::system_error);
}

TEST_F(ManagedLifecycleTest, ThreadInterfaceHasNoFallbackLaunchBackend) {
    EXPECT_TRUE(std::is_abstract_v<Thread>);
    EXPECT_FALSE(std::is_copy_constructible_v<ManagedThread>);
    EXPECT_FALSE(std::is_move_constructible_v<ManagedThread>);
    EXPECT_EQ(MetadataWorker{}.getName(), "Thread");
}

TEST_F(ManagedLifecycleTest, FailedAllocationLeavesTheWorkerReadyAndRetryPublishesOnce) {
    ASSERT_EXIT(
        {
            ::alarm(3);
            MetadataWorker worker;
            auto published = worker.published.get_future();
            {
                // Fail the first allocation, before a native worker exists.
                // End the single-threaded probe before a successful retry.
                AllocationProbe probe(1);
                bool threw = false;
                try {
                    worker.start();
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                if (!threw || !probe.rejected() || probe.outstanding() != 0 || worker.getStatus() != Thread::READY ||
                    worker.getTID() != TID{})
                    std::_Exit(1);
            }
            worker.start();
            const bool observed = published.get();
            worker.stop();
            worker.join();
            std::_Exit(observed && worker.getStatus() == Thread::EXIT && !ServerShutdown::failed.load() ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}
