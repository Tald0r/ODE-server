#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <sys/socket.h>

#include "DatagramSocket.h"
#include "ListenerStartup.h"
#include "ServerShutdown.h"
#include "ServerSocket.h"

using namespace std::chrono_literals;

namespace {

class ListenerStartupTest : public ::testing::Test {
protected:
    void SetUp() override {
        previousRequest = ServerShutdown::requested.exchange(false);
        previousFailure = ServerShutdown::failed.exchange(false);
    }

    void TearDown() override {
        ServerShutdown::requested.store(previousRequest);
        ServerShutdown::failed.store(previousFailure);
    }

    std::vector<std::string> events;
    int attempts = 0;

private:
    bool previousRequest = false;
    bool previousFailure = false;
};

TEST_F(ListenerStartupTest, SuccessfulBindingRunsOnceWithoutReportingOrRequestingShutdown) {
    de::retryListenerStartup([&] { ++attempts; }, [&](const BindException&) { events.emplace_back("failure"); }, "TCP");
    EXPECT_EQ(1, attempts);
    EXPECT_TRUE(events.empty());
    EXPECT_FALSE(ServerShutdown::isRequested());
    EXPECT_FALSE(ServerShutdown::failed.load());
}

TEST_F(ListenerStartupTest, EachBindFailureIsReportedBeforeTheNextAttempt) {
    de::retryListenerStartup(
        [&] {
            events.push_back("attempt " + std::to_string(++attempts));
            if (attempts < 3)
                throw BindException("busy " + std::to_string(attempts));
        },
        [&](const BindException& error) { events.push_back(error.toString()); }, "UDP", 0us);
    EXPECT_EQ((std::vector<std::string>{"attempt 1", BindException("busy 1").toString(), "attempt 2",
                                        BindException("busy 2").toString(), "attempt 3"}),
              events);
    EXPECT_FALSE(ServerShutdown::failed.load());
}

TEST_F(ListenerStartupTest, AnExistingShutdownRequestPreventsAnyAttemptOrReport) {
    ServerShutdown::request();
    for (const std::string transport : {"TCP", "UDP"}) {
        try {
            de::retryListenerStartup([&] { ++attempts; }, [&](const BindException&) { events.emplace_back("failure"); },
                                     transport);
            FAIL() << "listener opened during shutdown";
        } catch (const Error& error) {
            EXPECT_NE(std::string::npos,
                      error.toString().find("shutdown requested during " + transport + " listener startup"));
        }
    }
    EXPECT_EQ(0, attempts);
    EXPECT_TRUE(events.empty());
}

TEST_F(ListenerStartupTest, ShutdownDuringReportingPreventsTheWaitAndAnotherAttempt) {
    EXPECT_THROW(de::retryListenerStartup(
                     [&] {
                         ++attempts;
                         throw BindException("busy");
                     },
                     [](const BindException&) { ServerShutdown::request(); }, "TCP", 5s),
                 Error);
    EXPECT_EQ(1, attempts);
    EXPECT_FALSE(ServerShutdown::failed.load());
}

TEST_F(ListenerStartupTest, SuccessfulBindingReturnsEvenIfShutdownArrivesDuringTheAttempt) {
    EXPECT_NO_THROW(de::retryListenerStartup(
        [&] {
            ++attempts;
            ServerShutdown::request();
        },
        [&](const BindException&) { events.emplace_back("failure"); }, "TCP"));
    EXPECT_EQ(1, attempts);
    EXPECT_TRUE(events.empty());
    EXPECT_TRUE(ServerShutdown::isRequested());
}

TEST_F(ListenerStartupTest, ConfigurationErrorsEscapeWithoutBeingRetriedOrReportedAsBindFailures) {
    EXPECT_THROW(de::retryListenerStartup(
                     [&] {
                         ++attempts;
                         throw NoSuchElementException("TCPPort");
                     },
                     [&](const BindException&) { events.emplace_back("failure"); }, "TCP", 0us),
                 NoSuchElementException);
    EXPECT_EQ(1, attempts);
    EXPECT_TRUE(events.empty());
}

TEST_F(ListenerStartupTest, OtherStartupExceptionsEscapeWithoutRetry) {
    EXPECT_THROW(de::retryListenerStartup(
                     [&] {
                         ++attempts;
                         throw std::runtime_error("socket configuration failed");
                     },
                     [&](const BindException&) { events.emplace_back("failure"); }, "UDP", 0us),
                 std::runtime_error);
    EXPECT_EQ(1, attempts);
    EXPECT_TRUE(events.empty());
}

TEST_F(ListenerStartupTest, AReportingFailureEscapesEvenWhenItIsABindException) {
    EXPECT_THROW(de::retryListenerStartup(
                     [&] {
                         ++attempts;
                         throw BindException("busy");
                     },
                     [](const BindException&) { throw BindException("reporting failed"); }, "TCP", 0us),
                 BindException);
    EXPECT_EQ(1, attempts);
}

TEST_F(ListenerStartupTest, AnExistingWorkerFailureIsPreserved) {
    ServerShutdown::fail();
    EXPECT_THROW(de::retryListenerStartup([&] { ++attempts; }, [](const BindException&) {}, "TCP"), Error);
    EXPECT_EQ(0, attempts);
    EXPECT_TRUE(ServerShutdown::failed.load());
    EXPECT_TRUE(ServerShutdown::isRequested());
}

TEST_F(ListenerStartupTest, SuccessfulRetriesRespectTheRequestedDelay) {
    const auto started = std::chrono::steady_clock::now();
    de::retryListenerStartup(
        [&] {
            if (++attempts == 1)
                throw BindException("busy");
        },
        [](const BindException&) {}, "TCP", 25ms);
    EXPECT_EQ(2, attempts);
    EXPECT_GE(std::chrono::steady_clock::now() - started, 25ms);
}

TEST_F(ListenerStartupTest, AWorkerShutdownRequestInterruptsALongRetryDelay) {
    std::promise<void> reported;
    auto firstFailure = reported.get_future();
    std::jthread stopper([&] {
        if (firstFailure.wait_for(1s) == std::future_status::ready)
            std::this_thread::sleep_for(50ms);
        ServerShutdown::request();
    });
    const auto started = std::chrono::steady_clock::now();
    EXPECT_THROW(de::retryListenerStartup(
                     [&] {
                         ++attempts;
                         throw BindException("busy");
                     },
                     [&](const BindException&) { reported.set_value(); }, "UDP", 5s),
                 Error);
    EXPECT_LT(std::chrono::steady_clock::now() - started, 2s);
    EXPECT_EQ(1, attempts);
}

int reservePort(int type, unsigned short& port) {
    const int fd = ::socket(AF_INET, type, 0);
    if (fd < 0)
        return -1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t size = sizeof(address);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), size) != 0 || (type == SOCK_STREAM && ::listen(fd, 1) != 0) ||
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
        ::close(fd);
        return -1;
    }
    port = ntohs(address.sin_port);
    return fd;
}

void attemptListener(int type, unsigned short port) {
    if (type == SOCK_STREAM) {
        ServerSocket listener(port);
    } else {
        DatagramSocket listener(port);
    }
}

// Failed-construction regression tests run in children so the unfixed code's
// leaked descriptors cannot pollute the remaining tests.
class ListenerSocketTest : public ::testing::TestWithParam<int> {};

TEST_P(ListenerSocketTest, FailedConstructionReleasesItsDescriptorOnEveryAttempt) {
    ASSERT_EXIT(
        {
            unsigned short port = 0;
            const int occupied = reservePort(GetParam(), port);
            if (occupied < 0)
                std::_Exit(1);
            const int available = ::socket(AF_INET, GetParam(), 0);
            if (available < 0)
                std::_Exit(2);
            ::close(available);
            for (int i = 0; i < 8; ++i) {
                try {
                    attemptListener(GetParam(), port);
                    std::_Exit(3);
                } catch (const BindException&) {
                }
                const int next = ::socket(AF_INET, GetParam(), 0);
                if (next != available)
                    std::_Exit(4);
                ::close(next);
            }
            ::close(occupied);
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST_P(ListenerSocketTest, RealBindRetriesStopWithoutLeakingWhenShutdownIsRequested) {
    ASSERT_EXIT(
        {
            unsigned short port = 0;
            const int occupied = reservePort(GetParam(), port);
            const int available = ::socket(AF_INET, GetParam(), 0);
            if (occupied < 0 || available < 0)
                std::_Exit(1);
            ::close(available);
            ServerShutdown::requested.store(false);
            ServerShutdown::failed.store(false);
            int attempts = 0;
            int reports = 0;
            try {
                de::retryListenerStartup(
                    [&] {
                        ++attempts;
                        attemptListener(GetParam(), port);
                    },
                    [&](const BindException&) {
                        if (++reports == 3)
                            ServerShutdown::request();
                    },
                    "test", 0us);
                std::_Exit(2);
            } catch (const Error&) {
            }
            const int next = ::socket(AF_INET, GetParam(), 0);
            const bool intact = next == available && attempts == 3 && reports == 3 && !ServerShutdown::failed.load();
            ::close(next);
            ::close(occupied);
            std::_Exit(intact ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(Ports, ListenerSocketTest, ::testing::Values(SOCK_STREAM, SOCK_DGRAM));

} // namespace
