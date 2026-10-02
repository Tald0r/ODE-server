#include <pthread.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "ServerLifecycle.h"
#include "ServerProcessShutdown.h"

using namespace std::chrono_literals;

namespace {

volatile std::sig_atomic_t previousHandlerSignal = 0;

void previousHandler(int signal) {
    previousHandlerSignal = signal;
}

class ServerProcessShutdownTest : public ::testing::Test {
protected:
    void SetUp() override {
        previousRequest = ServerShutdown::requested.exchange(false);
        previousFailure = ServerShutdown::failed.exchange(false);
        previousHandlerSignal = 0;

        struct sigaction action {};
        action.sa_handler = previousHandler;
        action.sa_flags = SA_RESTART;
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGUSR1);
        savedTerm = sigaction(SIGTERM, &action, &originalTerm) == 0;
        ASSERT_TRUE(savedTerm);
        savedInt = sigaction(SIGINT, &action, &originalInt) == 0;
        ASSERT_TRUE(savedInt);

        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, SIGTERM);
        sigaddset(&signals, SIGINT);
        savedMask = pthread_sigmask(SIG_UNBLOCK, &signals, &originalMask) == 0;
        ASSERT_TRUE(savedMask);
    }

    void TearDown() override {
        if (savedInt)
            EXPECT_EQ(0, sigaction(SIGINT, &originalInt, nullptr));
        if (savedTerm)
            EXPECT_EQ(0, sigaction(SIGTERM, &originalTerm, nullptr));
        if (savedMask)
            EXPECT_EQ(0, pthread_sigmask(SIG_SETMASK, &originalMask, nullptr));
        ServerShutdown::requested.store(previousRequest);
        ServerShutdown::failed.store(previousFailure);
    }

    void expectShutdownHandler(int signal) {
        struct sigaction action {};
        ASSERT_EQ(0, sigaction(signal, nullptr, &action));
        EXPECT_EQ(ServerShutdown::request, action.sa_handler);
        EXPECT_EQ(0, action.sa_flags & (SA_RESTART | SA_SIGINFO));
        EXPECT_EQ(0, sigismember(&action.sa_mask, SIGUSR1));
    }

    void expectPreviousHandler(int signal) {
        struct sigaction action {};
        ASSERT_EQ(0, sigaction(signal, nullptr, &action));
        EXPECT_EQ(previousHandler, action.sa_handler);
        EXPECT_NE(0, action.sa_flags & SA_RESTART);
        EXPECT_EQ(1, sigismember(&action.sa_mask, SIGUSR1));
        ASSERT_EQ(0, std::raise(signal));
        EXPECT_EQ(signal, previousHandlerSignal);
    }

    bool previousRequest = false;
    bool previousFailure = false;
    bool savedTerm = false;
    bool savedInt = false;
    bool savedMask = false;
    struct sigaction originalTerm {};
    struct sigaction originalInt {};
    sigset_t originalMask{};
};

TEST_F(ServerProcessShutdownTest, InstallsBothHandlersWithoutChangingTheCallingThreadsSignalMask) {
    sigset_t before;
    ASSERT_EQ(0, pthread_sigmask(SIG_SETMASK, nullptr, &before));
    de::ServerProcessShutdown shutdown("testserver");
    ASSERT_TRUE(shutdown.ready());
    expectShutdownHandler(SIGTERM);
    expectShutdownHandler(SIGINT);
    EXPECT_FALSE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
    sigset_t after;
    ASSERT_EQ(0, pthread_sigmask(SIG_SETMASK, nullptr, &after));
    for (int signal : {SIGTERM, SIGINT, SIGUSR1})
        EXPECT_EQ(sigismember(&before, signal), sigismember(&after, signal));
}

TEST_F(ServerProcessShutdownTest, LeavingScopeRestoresThePreviousHandlersFlagsAndHandlerMasks) {
    {
        de::ServerProcessShutdown shutdown("testserver");
        ASSERT_TRUE(shutdown.ready());
    }
    expectPreviousHandler(SIGTERM);
    expectPreviousHandler(SIGINT);
    EXPECT_FALSE(ServerShutdown::isRequested());
}

TEST_F(ServerProcessShutdownTest, UnwindingRestoresHandlersAndCancelsTheWatcher) {
    EXPECT_THROW(
        {
            de::ServerProcessShutdown shutdown("testserver");
            ASSERT_TRUE(shutdown.ready());
            throw std::runtime_error("startup failed");
        },
        std::runtime_error);
    expectPreviousHandler(SIGTERM);
    expectPreviousHandler(SIGINT);
}

TEST_F(ServerProcessShutdownTest, AnExistingShutdownRequestIsPreserved) {
    ServerShutdown::request();
    {
        de::ServerProcessShutdown shutdown("testserver");
        ASSERT_TRUE(shutdown.ready());
        EXPECT_TRUE(ServerShutdown::isRequested());
    }
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
}

TEST_F(ServerProcessShutdownTest, AnExistingWorkerFailureIsPreserved) {
    ServerShutdown::fail();
    {
        de::ServerProcessShutdown shutdown("testserver");
        ASSERT_TRUE(shutdown.ready());
        EXPECT_TRUE(ServerShutdown::isRequested());
        EXPECT_TRUE(ServerShutdown::failed.load());
    }
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_TRUE(ServerShutdown::failed.load());
}

class ServerShutdownSignalTest : public ServerProcessShutdownTest, public ::testing::WithParamInterface<int> {};

TEST_P(ServerShutdownSignalTest, SignalOnMainRequestsShutdownWithoutMarkingFailure) {
    de::ServerProcessShutdown shutdown("testserver");
    ASSERT_TRUE(shutdown.ready());
    ASSERT_EQ(0, std::raise(GetParam()));
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
    EXPECT_EQ(0, previousHandlerSignal);
}

TEST_P(ServerShutdownSignalTest, SignalOnAWorkerReachesTheSharedShutdownState) {
    de::ServerProcessShutdown shutdown("testserver");
    ASSERT_TRUE(shutdown.ready());
    int signalResult = -1;
    std::jthread worker([&] { signalResult = std::raise(GetParam()); });
    worker.join();
    EXPECT_EQ(0, signalResult);
    EXPECT_TRUE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
    EXPECT_EQ(0, previousHandlerSignal);
}

TEST_P(ServerShutdownSignalTest, SignalDuringInitializationSkipsStartAndStillCleansUp) {
    de::ServerProcessShutdown shutdown("testserver");
    ASSERT_TRUE(shutdown.ready());
    std::vector<int> calls;
    std::ostringstream output;
    std::ostringstream errors;
    de::ServerLifecycleActions actions;
    actions.initialize = [&] {
        calls.push_back(1);
        EXPECT_EQ(0, std::raise(GetParam()));
    };
    actions.start = [&] { calls.push_back(2); };
    actions.stop = [&] {
        EXPECT_TRUE(ServerShutdown::isRequested());
        calls.push_back(3);
    };
    const auto result = de::runServerLifecycle(actions, output, errors);
    EXPECT_EQ((std::vector<int>{1, 3}), calls);
    EXPECT_TRUE(result.drained);
    EXPECT_EQ(EXIT_SUCCESS, result.exitCode);
    EXPECT_TRUE(output.str().empty());
    EXPECT_TRUE(errors.str().empty());
}

TEST_P(ServerShutdownSignalTest, SignalDuringStartReachesSuccessfulCleanup) {
    de::ServerProcessShutdown shutdown("testserver");
    ASSERT_TRUE(shutdown.ready());
    std::vector<int> calls;
    std::ostringstream output;
    std::ostringstream errors;
    de::ServerLifecycleActions actions;
    actions.initialize = [&] { calls.push_back(1); };
    actions.start = [&] {
        calls.push_back(2);
        EXPECT_EQ(0, std::raise(GetParam()));
    };
    actions.stop = [&] {
        EXPECT_TRUE(ServerShutdown::isRequested());
        calls.push_back(3);
    };
    const auto result = de::runServerLifecycle(actions, output, errors);
    EXPECT_EQ((std::vector<int>{1, 2, 3}), calls);
    EXPECT_TRUE(result.drained);
    EXPECT_EQ(EXIT_SUCCESS, result.exitCode);
}

INSTANTIATE_TEST_SUITE_P(Signals, ServerShutdownSignalTest, ::testing::Values(SIGTERM, SIGINT));

// A broken deadline must fail a subprocess check instead of hanging ctest.
[[noreturn]] void blockUntilDeadline() {
    std::this_thread::sleep_for(2s);
    std::_Exit(3);
}

class ServerProcessShutdownDeathTest : public ServerProcessShutdownTest {};

TEST_F(ServerProcessShutdownDeathTest, TheDeadlineDoesNotCountDownBeforeARequest) {
    ASSERT_EXIT(
        {
            de::ServerProcessShutdown shutdown("testserver", 50ms);
            if (!shutdown.ready())
                std::_Exit(2);
            std::this_thread::sleep_for(150ms);
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST_F(ServerProcessShutdownDeathTest, LeavingScopeCancelsAnArmedDeadline) {
    ASSERT_EXIT(
        {
            {
                de::ServerProcessShutdown shutdown("testserver", 1s);
                if (!shutdown.ready() || std::raise(SIGTERM) != 0)
                    std::_Exit(2);
            }
            std::this_thread::sleep_for(1100ms);
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST_F(ServerProcessShutdownDeathTest, AStartupFailureAlsoBoundsBlockedCleanupWithoutASignal) {
    ASSERT_EXIT(
        {
            de::ServerProcessShutdown shutdown("testserver", 50ms);
            if (!shutdown.ready())
                std::_Exit(2);
            std::ostringstream output;
            std::ostringstream errors;
            de::ServerLifecycleActions actions;
            actions.initialize = [] { throw std::runtime_error("startup failed"); };
            actions.start = [] { std::_Exit(2); };
            actions.stop = blockUntilDeadline;
            (void)de::runServerLifecycle(actions, output, errors);
            std::_Exit(2);
        },
        ::testing::ExitedWithCode(EXIT_FAILURE), "testserver shutdown deadline exceeded; forcing process exit");
}

class ServerShutdownSignalDeathTest : public ServerProcessShutdownTest, public ::testing::WithParamInterface<int> {};

TEST_P(ServerShutdownSignalDeathTest, TheDeadlineBoundsInitializationBlockedAfterASignal) {
    ASSERT_EXIT(
        {
            de::ServerProcessShutdown shutdown("testserver", 50ms);
            if (!shutdown.ready())
                std::_Exit(2);
            std::ostringstream output;
            std::ostringstream errors;
            de::ServerLifecycleActions actions;
            actions.initialize = [&] {
                if (std::raise(GetParam()) != 0)
                    std::_Exit(2);
                blockUntilDeadline();
            };
            actions.start = [] { std::_Exit(2); };
            actions.stop = [] { std::_Exit(2); };
            (void)de::runServerLifecycle(actions, output, errors);
            std::_Exit(2);
        },
        ::testing::ExitedWithCode(EXIT_FAILURE), "testserver shutdown deadline exceeded; forcing process exit");
}

TEST_P(ServerShutdownSignalDeathTest, TheDeadlineBoundsStopBlockedAfterASignal) {
    ASSERT_EXIT(
        {
            de::ServerProcessShutdown shutdown("testserver", 50ms);
            if (!shutdown.ready())
                std::_Exit(2);
            std::ostringstream output;
            std::ostringstream errors;
            de::ServerLifecycleActions actions;
            actions.initialize = [] {};
            actions.start = [&] {
                if (std::raise(GetParam()) != 0)
                    std::_Exit(2);
            };
            actions.stop = blockUntilDeadline;
            (void)de::runServerLifecycle(actions, output, errors);
            std::_Exit(2);
        },
        ::testing::ExitedWithCode(EXIT_FAILURE), "testserver shutdown deadline exceeded; forcing process exit");
}

INSTANTIATE_TEST_SUITE_P(Signals, ServerShutdownSignalDeathTest, ::testing::Values(SIGTERM, SIGINT));

} // namespace
