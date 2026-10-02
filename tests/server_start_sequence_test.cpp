#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "ManagedThread.h"
#include "ServerLifecycle.h"
#include "ServerStartSequence.h"
#include "ServerWorkerShutdown.h"

using namespace std::chrono_literals;

namespace {
class ServerStartSequenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        previousRequest = ServerShutdown::requested.exchange(false);
        previousFailure = ServerShutdown::failed.exchange(false);
    }

    void TearDown() override {
        ServerShutdown::requested.store(previousRequest);
        ServerShutdown::failed.store(previousFailure);
    }

    void step(int index) {
        calls.push_back(index);
        if (index == throwAt)
            std::rethrow_exception(failure);
        if (index == stopAt) {
            if (failOnStop)
                ServerShutdown::fail();
            else
                ServerShutdown::request();
        }
    }

    void run() {
        const de::ServerStartAction background[] = {[this] { step(0); }, [this] { step(1); }, [this] { step(2); }};
        de::runServerStartSequence(background, [this] { step(3); });
    }

    std::vector<int> calls;
    int stopAt = -2;
    bool failOnStop = false;
    int throwAt = -2;
    std::exception_ptr failure;

private:
    bool previousRequest = false;
    bool previousFailure = false;
};

TEST_F(ServerStartSequenceTest, BackgroundStepsPrecedeTheMainLoopAndNormalReturnDoesNotRequestShutdown) {
    run();
    EXPECT_EQ((std::vector<int>{0, 1, 2, 3}), calls);
    EXPECT_FALSE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
}

TEST_F(ServerStartSequenceTest, EmptyBackgroundGroupStillRunsTheMainLoop) {
    de::runServerStartSequence({}, [this] { step(3); });
    EXPECT_EQ((std::vector<int>{3}), calls);
    EXPECT_FALSE(ServerShutdown::isRequested());
}

TEST_F(ServerStartSequenceTest, EmptyBackgroundGroupAlsoObservesShutdownBeforeTheMainLoop) {
    ServerShutdown::request();
    de::runServerStartSequence({}, [this] { step(3); });
    EXPECT_TRUE(calls.empty());
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
}

class ShutdownBoundaryTest : public ServerStartSequenceTest,
                             public ::testing::WithParamInterface<std::tuple<int, bool>> {};

TEST_P(ShutdownBoundaryTest, ARequestSkipsLaterStepsWithoutChangingItsFailureStatus) {
    std::tie(stopAt, failOnStop) = GetParam();
    if (stopAt == -1) {
        if (failOnStop)
            ServerShutdown::fail();
        else
            ServerShutdown::request();
    }
    run();
    std::vector<int> expected{0, 1, 2, 3};
    expected.resize(stopAt + 1);
    EXPECT_EQ(expected, calls);
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_EQ(failOnStop, ServerShutdown::failed.load());
}

INSTANTIATE_TEST_SUITE_P(BeforeAndDuringEachAction, ShutdownBoundaryTest,
                         ::testing::Combine(::testing::Values(-1, 0, 1, 2, 3), ::testing::Bool()));

class StartExceptionTest : public ServerStartSequenceTest,
                           public ::testing::WithParamInterface<std::tuple<int, int>> {};

TEST_P(StartExceptionTest, TheOriginalExceptionEscapesAndLaterStepsNeverRun) {
    const auto [index, kind] = GetParam();
    throwAt = index;
    switch (kind) {
    case 0:
        failure = std::make_exception_ptr(Error("start failed"));
        break;
    case 1:
        failure = std::make_exception_ptr(std::runtime_error("start failed"));
        break;
    default:
        failure = std::make_exception_ptr(42);
        break;
    }
    try {
        run();
        FAIL() << "The startup exception was lost";
    } catch (...) {
        EXPECT_EQ(failure, std::current_exception());
    }
    std::vector<int> expected{0, 1, 2, 3};
    expected.resize(index + 1);
    EXPECT_EQ(expected, calls);
    // Failure status and draining belong to ServerLifecycle, not this helper.
    EXPECT_FALSE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
}

INSTANTIATE_TEST_SUITE_P(EveryActionAndExceptionKind, StartExceptionTest,
                         ::testing::Combine(::testing::Values(0, 1, 2, 3), ::testing::Values(0, 1, 2)));

class StartupWorker : public ManagedThread {
public:
    ~StartupWorker() noexcept override {
        ManagedThread::stop();
        ManagedThread::join();
    }

    void start() override {
        ++starts;
        if (rejectStart)
            throw std::runtime_error("worker start failed");
        ManagedThread::start();
    }

    void run() override {
        entered.set_value();
        if (failRun) {
            finished = true;
            throw std::runtime_error("worker run failed");
        }
        while (pauseFor(5s)) {
        }
        finished = true;
    }

    void stop() override {
        ++stops;
        ManagedThread::stop();
    }

    void join() override {
        ++joins;
        ManagedThread::join();
    }

    unsigned starts = 0, stops = 0, joins = 0;
    bool rejectStart = false, failRun = false;
    std::promise<void> entered;
    std::atomic<bool> finished{false};
};

enum class Outcome { Normal, ShutdownBetweenStarts, WorkerFailure, StartThrows, MainThrows };
class StartupLifecycleTest : public ServerStartSequenceTest, public ::testing::WithParamInterface<Outcome> {};

TEST_P(StartupLifecycleTest, RealWorkersDrainEvenWhenOnlyPartOfStartupRuns) {
    const auto outcome = GetParam();
    StartupWorker first, second;
    first.failRun = outcome == Outcome::WorkerFailure;
    second.rejectStart = outcome == Outcome::StartThrows;
    auto firstEntered = first.entered.get_future();
    auto secondEntered = second.entered.get_future();
    const de::ServerStartAction background[] = {
        [&] {
            first.start();
            if (firstEntered.wait_for(2s) != std::future_status::ready)
                throw std::runtime_error("first worker did not enter");
            if (outcome == Outcome::ShutdownBetweenStarts)
                ServerShutdown::request();
            if (outcome == Outcome::WorkerFailure)
                first.ManagedThread::join(); // Observe the real asynchronous failure before the next step.
        },
        [&] {
            second.start();
            if (secondEntered.wait_for(2s) != std::future_status::ready)
                throw std::runtime_error("second worker did not enter");
        },
    };
    const de::ServerWorker workers[] = {{first, "first"}, {second, "second"}};
    unsigned mainRuns = 0, drains = 0;
    std::ostringstream output, errors;
    const de::ServerLifecycleActions lifecycle{
        [] {},
        [&] {
            de::runServerStartSequence(background, [&] {
                ++mainRuns;
                if (outcome == Outcome::MainThrows)
                    throw std::runtime_error("main loop failed");
            });
        },
        [&] {
            EXPECT_TRUE(ServerShutdown::isRequested());
            ++drains;
            de::stopServerWorkers(workers, errors);
        },
    };
    const auto result = de::runServerLifecycle(lifecycle, output, errors);
    const bool stopsEarly = outcome == Outcome::ShutdownBetweenStarts || outcome == Outcome::WorkerFailure;
    const bool runsMain = outcome == Outcome::Normal || outcome == Outcome::MainThrows;
    const bool failed = outcome != Outcome::Normal && outcome != Outcome::ShutdownBetweenStarts;
    EXPECT_TRUE(result.drained);
    EXPECT_EQ(failed ? EXIT_FAILURE : EXIT_SUCCESS, result.exitCode);
    EXPECT_EQ(failed, ServerShutdown::failed.load());
    EXPECT_EQ(1u, first.starts);
    EXPECT_EQ(stopsEarly ? 0u : 1u, second.starts);
    EXPECT_EQ(runsMain ? 1u : 0u, mainRuns);
    EXPECT_EQ(1u, drains);
    EXPECT_EQ(1u, first.stops);
    EXPECT_EQ(1u, second.stops);
    EXPECT_EQ(1u, first.joins);
    EXPECT_EQ(1u, second.joins);
    EXPECT_TRUE(first.finished.load());
    EXPECT_EQ(runsMain, second.finished.load());
    if (outcome == Outcome::WorkerFailure)
        EXPECT_NE(std::string::npos, errors.str().find("first: worker run failed"));
    else
        EXPECT_TRUE(errors.str().empty());
}

INSTANTIATE_TEST_SUITE_P(StartupOutcomes, StartupLifecycleTest,
                         ::testing::Values(Outcome::Normal, Outcome::ShutdownBetweenStarts, Outcome::WorkerFailure,
                                           Outcome::StartThrows, Outcome::MainThrows));
} // namespace
