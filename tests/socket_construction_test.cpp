#include <unistd.h>

#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include "Player.h"
#include "ServerSocket.h"
#include "Socket.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
std::uint16_t portOf(ServerSocket& listener) {
    sockaddr_in address{};
    socklen_t size = sizeof(address);
    if (::getsockname(listener.getSOCKET(), reinterpret_cast<sockaddr*>(&address), &size) != 0)
        std::_Exit(95);
    return ntohs(address.sin_port);
}

bool readable(int descriptor) {
    pollfd ready{descriptor, POLLIN, 0};
    return ::poll(&ready, 1, 2000) == 1 && (ready.revents & (POLLIN | POLLHUP));
}

bool openSocket(int descriptor) {
    int type = 0;
    socklen_t size = sizeof(type);
    return ::getsockopt(descriptor, SOL_SOCKET, SO_TYPE, &type, &size) == 0;
}

class SocketConstructionTest : public ::testing::TestWithParam<bool> {};

TEST_P(SocketConstructionTest, DescriptorExhaustionDoesNotLeakTheImplementation) {
    ASSERT_EXIT(
        {
            rlimit original{};
            if (::getrlimit(RLIMIT_NOFILE, &original) != 0)
                std::_Exit(1);
            rlimit unavailable = original;
            unavailable.rlim_cur = 0;
            if (::setrlimit(RLIMIT_NOFILE, &unavailable) != 0)
                std::_Exit(2);
            AllocationProbe probe;
            bool threw = false;
            try {
                if (GetParam()) {
                    Socket socket("127.0.0.1", 1234);
                } else {
                    Socket socket;
                }
            } catch (const Error&) {
                threw = true;
            }
            if (::setrlimit(RLIMIT_NOFILE, &original) != 0)
                std::_Exit(3);
            std::_Exit(threw && probe.attempts() > 0 && probe.outstanding() == 0 ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(Constructors, SocketConstructionTest, ::testing::Bool());

TEST(PlayerConstruction, EveryAllocationFailureReleasesTheAdoptedSocketAndPartialStreams) {
    // Sweep past the successful operation's allocation count as well: the
    // final iterations verify normal ownership and destruction. Each child
    // has its own fault injection and descriptor table.
    for (std::size_t failAt = 1; failAt <= 12; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto socket = std::make_unique<Socket>();
                    std::unique_ptr<Player> player(new Player(socket.release()));
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == available && (failAt != 12 || !probe.rejected());
                std::_Exit(intact ? 0 : 5);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(SocketAdoption, AcceptAllocationFailuresReleaseTheDescriptorAndImplementation) {
    for (std::size_t failAt = 1; failAt <= 8; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                ServerSocket listener(0);
                Socket peer("127.0.0.1", portOf(listener));
                peer.connect();
                if (!readable(listener.getSOCKET()))
                    std::_Exit(1);
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    std::unique_ptr<Socket> accepted(listener.accept());
                    if (!accepted)
                        std::_Exit(2);
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == available && (failAt != 8 || !probe.rejected());
                std::_Exit(intact ? 0 : 3);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(SocketAdoption, AnEmptyNonblockingAcceptStillReturnsNullWithoutLeaking) {
    ServerSocket listener(0);
    listener.setNonBlocking(true);
    const int available = nextSocketDescriptor();
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::unique_ptr<Socket> accepted(listener.accept());
        EXPECT_EQ(nullptr, accepted);
        EXPECT_EQ(available, nextSocketDescriptor());
    }
}

TEST(SocketAdoption, ReconnectAllocationFailuresLeaveASafeClosedSocketThatCanRetry) {
    for (std::size_t failAt = 1; failAt <= 8; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                LoopbackListener listener;
                auto socket = std::make_unique<Socket>("127.0.0.1", 1234);
                const int oldDescriptor = socket->getSOCKET();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    socket->reconnect("127.0.0.1", listener.port());
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                probe.stopFailing();
                if (threw) {
                    if (probe.outstanding() != 0 || socket->getPort() != 1234 || openSocket(oldDescriptor))
                        std::_Exit(4);
                    const int reused = ::socket(AF_INET, SOCK_STREAM, 0);
                    socket->close();
                    if (reused != oldDescriptor || !openSocket(reused))
                        std::_Exit(5);
                    socket->reconnect("127.0.0.1", listener.port());
                    if (!openSocket(reused))
                        std::_Exit(6);
                    ::close(reused);
                }
                if (socket->getPort() != listener.port() || !openSocket(socket->getSOCKET()))
                    std::_Exit(7);
                socket.reset();
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == oldDescriptor && (failAt != 8 || !probe.rejected());
                std::_Exit(intact ? 0 : 8);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(SocketAdoption, RefusedReconnectReleasesItsAttemptAndCanConnectToAnotherEndpoint) {
    std::uint16_t unavailablePort;
    {
        LoopbackListener unavailable;
        unavailablePort = unavailable.port();
    }
    Socket socket("127.0.0.1", 1234);
    const int oldDescriptor = socket.getSOCKET();
    EXPECT_THROW(socket.reconnect("127.0.0.1", unavailablePort), ConnectException);
    EXPECT_EQ(1234u, socket.getPort());
    EXPECT_FALSE(openSocket(oldDescriptor));
    EXPECT_EQ(oldDescriptor, nextSocketDescriptor());
    LoopbackListener ready;
    EXPECT_NO_THROW(socket.reconnect("127.0.0.1", ready.port()));
    EXPECT_EQ(ready.port(), socket.getPort());
}

TEST(SocketAdoption, SuccessfulReconnectClosesTheOldPeerAndExchangesBytesWithTheNewPeer) {
    ServerSocket original(0), replacement(0);
    Socket socket("127.0.0.1", portOf(original));
    socket.connect();
    ASSERT_TRUE(readable(original.getSOCKET()));
    std::unique_ptr<Socket> oldPeer(original.accept());
    ASSERT_NE(nullptr, oldPeer);
    socket.reconnect("127.0.0.1", portOf(replacement));
    ASSERT_TRUE(readable(replacement.getSOCKET()));
    std::unique_ptr<Socket> newPeer(replacement.accept());
    ASSERT_NE(nullptr, newPeer);
    ASSERT_TRUE(readable(oldPeer->getSOCKET()));
    char byte;
    EXPECT_THROW(oldPeer->receive(&byte, 1), ConnectException);
    EXPECT_EQ(1u, socket.send("x", 1));
    ASSERT_TRUE(readable(newPeer->getSOCKET()));
    ASSERT_EQ(1u, newPeer->receive(&byte, 1));
    EXPECT_EQ('x', byte);
}
} // namespace
