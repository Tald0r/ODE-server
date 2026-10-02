#include <fcntl.h>

#include <memory>

#include <gtest/gtest.h>

#include "AcceptedServerConnection.h"
#include "ServerSocket.h"
#include "support/LoopbackListener.h"

namespace {
std::uint16_t portOf(int descriptor) {
    sockaddr_in address{};
    socklen_t size = sizeof(address);
    if (::getsockname(descriptor, reinterpret_cast<sockaddr*>(&address), &size) != 0)
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

TEST(AcceptedServerConnection, EmptyAcceptRemainsEmpty) {
    const int available = nextSocketDescriptor();
    EXPECT_EQ(nullptr, de::prepareAcceptedServerConnection(nullptr));
    EXPECT_EQ(available, nextSocketDescriptor());
}

TEST(AcceptedServerConnection, PreservesThePeerAndConfiguresNonblockingIoWithoutLinger) {
    ServerSocket listener(0);
    Socket peer("127.0.0.1", portOf(listener.getSOCKET()));
    peer.connect();
    ASSERT_TRUE(readable(listener.getSOCKET()));
    std::unique_ptr<Socket> accepted(listener.accept());
    ASSERT_NE(nullptr, accepted);
    const int descriptor = accepted->getSOCKET();
    accepted->setNonBlocking(false);
    accepted->setLinger(8);
    auto prepared = de::prepareAcceptedServerConnection(std::move(accepted));
    EXPECT_EQ(nullptr, accepted);
    ASSERT_NE(nullptr, prepared);
    EXPECT_EQ(descriptor, prepared->getSOCKET());
    EXPECT_EQ("127.0.0.1", prepared->getHost());
    EXPECT_EQ(portOf(peer.getSOCKET()), prepared->getPort());
    const int flags = ::fcntl(descriptor, F_GETFL);
    ASSERT_GE(flags, 0);
    EXPECT_NE(0, flags & O_NONBLOCK);
    linger option{};
    socklen_t size = sizeof(option);
    ASSERT_EQ(0, ::getsockopt(descriptor, SOL_SOCKET, SO_LINGER, &option, &size));
    EXPECT_EQ(0, option.l_onoff);

    ASSERT_EQ(1u, prepared->send("a", 1));
    ASSERT_TRUE(readable(peer.getSOCKET()));
    char byte;
    ASSERT_EQ(1u, peer.receive(&byte, 1));
    EXPECT_EQ('a', byte);
    ASSERT_EQ(1u, peer.send("b", 1));
    ASSERT_TRUE(readable(descriptor));
    ASSERT_EQ(1u, prepared->receive(&byte, 1));
    EXPECT_EQ('b', byte);
    prepared.reset();
    EXPECT_FALSE(openSocket(descriptor));
    ASSERT_TRUE(readable(peer.getSOCKET()));
    EXPECT_EQ(0, ::recv(peer.getSOCKET(), &byte, 1, 0));
}

TEST(AcceptedServerConnection, ClosedSocketIsRejectedAndOwnershipIsConsumed) {
    auto socket = std::make_unique<Socket>();
    const int descriptor = socket->getSOCKET();
    socket->close();
    EXPECT_THROW(de::prepareAcceptedServerConnection(std::move(socket)), Error);
    EXPECT_EQ(nullptr, socket);
    EXPECT_FALSE(openSocket(descriptor));
}

TEST(AcceptedServerConnection, PendingPeerErrorRejectsAndReleasesTheConnection) {
    ServerSocket listener(0);
    Socket peer("127.0.0.1", portOf(listener.getSOCKET()));
    peer.connect();
    ASSERT_TRUE(readable(listener.getSOCKET()));
    std::unique_ptr<Socket> accepted(listener.accept());
    ASSERT_NE(nullptr, accepted);
    const int descriptor = accepted->getSOCKET();
    linger reset{1, 0};
    ASSERT_EQ(0, ::setsockopt(peer.getSOCKET(), SOL_SOCKET, SO_LINGER, &reset, sizeof(reset)));
    peer.close();
    ASSERT_TRUE(readable(descriptor));
    EXPECT_THROW(de::prepareAcceptedServerConnection(std::move(accepted)), Error);
    EXPECT_EQ(nullptr, accepted);
    EXPECT_FALSE(openSocket(descriptor));
}
} // namespace
