#include <poll.h>
#include <unistd.h>

#include <cstdlib>
#include <memory>
#include <new>
#include <string>

#include <gtest/gtest.h>
#include <sys/socket.h>

#include "OutboundServerConnection.h"
#include "Player.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
class AcceptedPeer {
public:
    explicit AcceptedPeer(const LoopbackListener& listener) : fd(listener.accept()) {}
    ~AcceptedPeer() {
        ::close(fd);
    }
    int fd;
};

bool readable(int fd) {
    pollfd ready{fd, POLLIN, 0};
    return ::poll(&ready, 1, 2000) == 1 && (ready.revents & (POLLIN | POLLHUP));
}

TEST(OutboundServerConnection, ReturnsTheConnectedEndpointWithNonblockingIoAndLingerDisabled) {
    LoopbackListener listener;
    auto socket = de::connectOutboundServer("127.0.0.1", listener.port());
    AcceptedPeer peer(listener);
    EXPECT_EQ("127.0.0.1", socket->getHost());
    EXPECT_EQ(listener.port(), socket->getPort());
    EXPECT_TRUE(socket->isNonBlocking());
    linger option{};
    socklen_t size = sizeof(option);
    ASSERT_EQ(0, ::getsockopt(socket->getSOCKET(), SOL_SOCKET, SO_LINGER, &option, &size));
    EXPECT_EQ(0, option.l_onoff);
    sockaddr_in address{};
    size = sizeof(address);
    ASSERT_EQ(0, ::getpeername(socket->getSOCKET(), reinterpret_cast<sockaddr*>(&address), &size));
    EXPECT_EQ(htonl(INADDR_LOOPBACK), address.sin_addr.s_addr);
    EXPECT_EQ(listener.port(), ntohs(address.sin_port));
}

TEST(OutboundServerConnection, ExchangesBytesInBothDirectionsWithoutAssumingImmediateDelivery) {
    LoopbackListener listener;
    auto socket = de::connectOutboundServer("127.0.0.1", listener.port());
    AcceptedPeer peer(listener);
    const std::string payload("one\0two", 7);
    ASSERT_EQ(payload.size(), socket->send(payload.data(), payload.size()));
    std::string received(payload.size(), '\0');
    for (std::size_t offset = 0; offset < received.size();) {
        ASSERT_TRUE(readable(peer.fd));
        const auto count = ::recv(peer.fd, received.data() + offset, received.size() - offset, 0);
        ASSERT_GT(count, 0);
        offset += count;
    }
    EXPECT_EQ(payload, received);

    ASSERT_EQ(static_cast<ssize_t>(payload.size()), ::send(peer.fd, payload.data(), payload.size(), 0));
    received.assign(payload.size(), '\0');
    for (std::size_t offset = 0; offset < received.size();) {
        ASSERT_TRUE(readable(socket->getSOCKET()));
        const auto count = socket->receive(received.data() + offset, received.size() - offset);
        ASSERT_GT(count, 0u);
        offset += count;
    }
    EXPECT_EQ(payload, received);
}

TEST(OutboundServerConnection, OwnershipCanTransferToAPlayerAndClosesTheConnectionAtDestruction) {
    LoopbackListener listener;
    auto socket = de::connectOutboundServer("127.0.0.1", listener.port());
    const int fd = socket->getSOCKET();
    AcceptedPeer peer(listener);
    {
        Player player(socket.release());
        EXPECT_EQ(nullptr, socket);
        EXPECT_EQ(fd, player.getSocket()->getSOCKET());
    }
    ASSERT_TRUE(readable(peer.fd));
    char byte;
    EXPECT_EQ(0, ::recv(peer.fd, &byte, 1, 0));
}

TEST(OutboundServerConnection, RefusedAttemptsPropagateAndReleaseEveryDescriptor) {
    // Close the listener before connecting. A bound but non-listening socket
    // drops connection attempts on macOS instead of refusing them promptly.
    std::uint16_t unavailablePort;
    {
        LoopbackListener unavailable;
        unavailablePort = unavailable.port();
    }
    const int available = nextSocketDescriptor();
    for (int attempt = 0; attempt < 8; ++attempt) {
        EXPECT_THROW(de::connectOutboundServer("127.0.0.1", unavailablePort), ConnectException);
        EXPECT_EQ(available, nextSocketDescriptor());
    }
    LoopbackListener ready;
    EXPECT_NO_THROW(de::connectOutboundServer("127.0.0.1", ready.port()));
}

TEST(OutboundServerConnection, AllocationFailuresLeaveNoSocketOrHeapAllocationBehind) {
    for (std::size_t failAt = 1; failAt <= 8; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                LoopbackListener listener;
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto socket = de::connectOutboundServer("127.0.0.1", listener.port());
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == available && (failAt != 8 || !probe.rejected());
                std::_Exit(intact ? 0 : 1);
            },
            ::testing::ExitedWithCode(0), "");
    }
}
} // namespace
