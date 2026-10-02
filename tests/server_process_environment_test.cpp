#include <pthread.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <limits>
#include <thread>

#include <gtest/gtest.h>
#include <sys/resource.h>

#include "ServerProcessEnvironment.h"
#include "ServerProcessShutdown.h"

namespace {

// Every mutation of resource limits, signal dispositions and rand() happens
// inside a death-test child. In particular, lowering a hard limit is irreversible
// for an ordinary process and must never affect the test runner.
class CoreDumpLimitTest : public ::testing::TestWithParam<rlim_t> {};

TEST_P(CoreDumpLimitTest, RaisesTheSoftLimitWithoutChangingAFiniteHardLimit) {
    struct rlimit inherited {};
    ASSERT_EQ(0, getrlimit(RLIMIT_CORE, &inherited));
    if (inherited.rlim_max < GetParam())
        GTEST_SKIP() << "inherited core hard limit is below this test's finite cap";

    ASSERT_EXIT(
        {
            const auto check = [&] {
                struct rlimit capped {};
                capped.rlim_max = GetParam();
                if (setrlimit(RLIMIT_CORE, &capped) != 0)
                    return 1;
                if (de::raiseCoreDumpLimit())
                    return 2;
                struct rlimit actual {};
                if (getrlimit(RLIMIT_CORE, &actual) != 0)
                    return 3;
                if (actual.rlim_cur != GetParam() || actual.rlim_max != GetParam())
                    return 4;
                if (de::raiseCoreDumpLimit() || getrlimit(RLIMIT_CORE, &actual) != 0)
                    return 5;
                return actual.rlim_cur == GetParam() && actual.rlim_max == GetParam() ? 0 : 6;
            };
            std::_Exit(check());
        },
        ::testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(Caps, CoreDumpLimitTest, ::testing::Values(rlim_t{0}, rlim_t{4096}, rlim_t{1024 * 1024}));

TEST(ServerProcessEnvironment, EnablesUnlimitedDumpsWhenTheInheritedHardLimitIsUnlimited) {
    struct rlimit inherited {};
    ASSERT_EQ(0, getrlimit(RLIMIT_CORE, &inherited));
    if (inherited.rlim_max != RLIM_INFINITY)
        GTEST_SKIP() << "inherited core hard limit is finite";

    ASSERT_EXIT(
        {
            struct rlimit disabled {};
            disabled.rlim_max = RLIM_INFINITY;
            if (setrlimit(RLIMIT_CORE, &disabled) != 0 || de::raiseCoreDumpLimit())
                std::_Exit(1);
            struct rlimit actual {};
            if (getrlimit(RLIMIT_CORE, &actual) != 0)
                std::_Exit(2);
            std::_Exit(actual.rlim_cur == RLIM_INFINITY && actual.rlim_max == RLIM_INFINITY ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ServerProcessEnvironment, CoreDumpSetupLeavesOtherResourceLimitsAlone) {
    ASSERT_EXIT(
        {
            struct rlimit before {};
            struct rlimit after {};
            if (getrlimit(RLIMIT_NOFILE, &before) != 0 || de::raiseCoreDumpLimit() ||
                getrlimit(RLIMIT_NOFILE, &after) != 0)
                std::_Exit(1);
            std::_Exit(before.rlim_cur == after.rlim_cur && before.rlim_max == after.rlim_max ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

std::array<int, 8> randomSequence() {
    std::array<int, 8> result;
    for (auto& value : result)
        value = std::rand();
    return result;
}

class ProcessRandomSeedTest : public ::testing::TestWithParam<unsigned int> {};

TEST_P(ProcessRandomSeedTest, ExplicitSeedsReproduceThePlatformRandomSequence) {
    ASSERT_EXIT(
        {
            // Compare to this platform's C library; the rand algorithm is not
            // required to be identical between Linux and macOS.
            std::srand(GetParam());
            const auto expected = randomSequence();
            de::seedProcessRandomness(GetParam());
            if (randomSequence() != expected)
                std::_Exit(1);
            de::seedProcessRandomness(GetParam());
            std::_Exit(randomSequence() == expected ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(Seeds, ProcessRandomSeedTest,
                         ::testing::Values(0U, 1U, 42U, std::numeric_limits<unsigned int>::max()));

TEST(ServerProcessEnvironment, GameInitializationReseedsAfterEarlierStartupWork) {
    ASSERT_EXIT(
        {
            std::srand(1234U);
            const auto expected = randomSequence();
            de::seedProcessRandomness(17U);
            (void)randomSequence();
            de::initializeGameProcess(1234U);
            std::_Exit(randomSequence() == expected ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

volatile std::sig_atomic_t receivedSignal = 0;

void previousHandler(int signal) {
    receivedSignal = signal;
}

class GameIgnoredSignalTest : public ::testing::TestWithParam<int> {};

TEST_P(GameIgnoredSignalTest, IgnoresTheSignalOnTheMainThread) {
    ASSERT_EXIT(
        {
            if (std::signal(GetParam(), previousHandler) == SIG_ERR)
                std::_Exit(1);
            de::initializeGameProcess(42U);
            struct sigaction actual {};
            if (sigaction(GetParam(), nullptr, &actual) != 0 || actual.sa_handler != SIG_IGN)
                std::_Exit(2);
            if (std::raise(GetParam()) != 0)
                std::_Exit(3);
            std::_Exit(receivedSignal == 0 ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST_P(GameIgnoredSignalTest, IgnoresTheSignalOnAWorkerThread) {
    ASSERT_EXIT(
        {
            if (std::signal(GetParam(), previousHandler) == SIG_ERR)
                std::_Exit(1);
            de::initializeGameProcess(42U);
            int delivered = -1;
            std::thread worker([&] { delivered = pthread_kill(pthread_self(), GetParam()); });
            worker.join();
            std::_Exit(delivered == 0 && receivedSignal == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(Signals, GameIgnoredSignalTest, ::testing::Values(SIGPIPE, SIGALRM, SIGCHLD));

TEST(ServerProcessEnvironment, AClosedPipeReportsEpipeInsteadOfTerminatingTheGameProcess) {
    ASSERT_EXIT(
        {
            if (std::signal(SIGPIPE, SIG_DFL) == SIG_ERR)
                std::_Exit(1);
            int pipeFDs[2];
            if (pipe(pipeFDs) != 0 || close(pipeFDs[0]) != 0)
                std::_Exit(2);
            de::initializeGameProcess(42U);
            const auto written = write(pipeFDs[1], "x", 1);
            const int writeError = errno;
            close(pipeFDs[1]);
            std::_Exit(written == -1 && writeError == EPIPE ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ServerProcessEnvironment, GameInitializationPreservesUnrelatedHandlersAndTheCallingThreadMask) {
    ASSERT_EXIT(
        {
            const auto check = [] {
                struct sigaction expected {};
                expected.sa_handler = previousHandler;
                expected.sa_flags = SA_RESTART;
                sigemptyset(&expected.sa_mask);
                sigaddset(&expected.sa_mask, SIGUSR1);
                for (int signal : {SIGTERM, SIGINT, SIGUSR2}) {
                    if (sigaction(signal, &expected, nullptr) != 0)
                        return 1;
                }
                sigset_t block;
                sigemptyset(&block);
                sigaddset(&block, SIGUSR1);
                if (pthread_sigmask(SIG_BLOCK, &block, nullptr) != 0)
                    return 2;
                sigset_t before;
                sigset_t after;
                if (pthread_sigmask(SIG_SETMASK, nullptr, &before) != 0)
                    return 3;
                de::initializeGameProcess(42U);
                if (pthread_sigmask(SIG_SETMASK, nullptr, &after) != 0)
                    return 4;
                for (int signal : {SIGTERM, SIGINT, SIGUSR1, SIGUSR2, SIGPIPE, SIGALRM, SIGCHLD}) {
                    if (sigismember(&before, signal) != sigismember(&after, signal))
                        return 5;
                }
                for (int signal : {SIGTERM, SIGINT, SIGUSR2}) {
                    struct sigaction actual {};
                    if (sigaction(signal, nullptr, &actual) != 0 || actual.sa_handler != previousHandler ||
                        !(actual.sa_flags & SA_RESTART) || sigismember(&actual.sa_mask, SIGUSR1) != 1)
                        return 6;
                }
                return 0;
            };
            std::_Exit(check());
        },
        ::testing::ExitedWithCode(0), "");
}

class GameShutdownSignalTest : public ::testing::TestWithParam<int> {};

TEST_P(GameShutdownSignalTest, GameInitializationKeepsTheProcessShutdownGuardEffective) {
    ASSERT_EXIT(
        {
            de::ServerProcessShutdown shutdown("environment-test");
            if (!shutdown.ready())
                std::_Exit(1);
            de::initializeGameProcess(42U);
            if (ServerShutdown::isRequested() || ServerShutdown::failed.load())
                std::_Exit(2);
            if (std::raise(GetParam()) != 0)
                std::_Exit(3);
            std::_Exit(ServerShutdown::isRequested() && !ServerShutdown::failed.load() ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(Signals, GameShutdownSignalTest, ::testing::Values(SIGTERM, SIGINT));

TEST(ServerProcessEnvironment, GameInitializationDoesNotClearAnExistingWorkerFailure) {
    ASSERT_EXIT(
        {
            ServerShutdown::fail();
            de::initializeGameProcess(42U);
            std::_Exit(ServerShutdown::isRequested() && ServerShutdown::failed.load() ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

} // namespace
