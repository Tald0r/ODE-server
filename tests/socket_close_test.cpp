// SocketImpl::close() runs twice on every player socket: Player's destructor
// closes the socket and then deletes it, and ~SocketImpl closes again. The
// second close must not release the descriptor number a second time, because
// by then another thread may already own that number. The number itself must
// stay readable after close(): the player managers index their tables by it
// and delete a player by getSOCKET() after disconnect() has closed the socket.
#include <unistd.h>

#include <gtest/gtest.h>
#include <sys/socket.h>

#include "Socket.h"
#include "SocketAPI.h"
#include "SocketImpl.h"

namespace {
bool isOpen(int fd) {
    int type = 0;
    socklen_t length = sizeof(type);
    return ::getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) == 0;
}
} // namespace

TEST(SocketImpl, OrderlyCloseIsDistinctFromOtherConnectionFailures) {
    int descriptors[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors), 0);
    ASSERT_EQ(::close(descriptors[1]), 0);
    char byte;
    EXPECT_THROW(SocketAPI::recv_ex(descriptors[0], &byte, 1, 0), PeerClosedException);
    // Existing callers that retire all lost connections keep their behavior.
    EXPECT_THROW(SocketAPI::recv_ex(descriptors[0], &byte, 1, 0), ConnectException);
    EXPECT_EQ(::close(descriptors[0]), 0);
}

TEST(SocketImpl, CloseReleasesTheDescriptorOnceAndKeepsItsNumber) {
    SocketImpl socket("127.0.0.1", 9);
    socket.create();
    const int fd = socket.getSOCKET();
    ASSERT_TRUE(socket.isValid());
    ASSERT_TRUE(isOpen(fd));
    socket.close();
    EXPECT_FALSE(isOpen(fd));
    // The managers still need the number to find the player's slot.
    EXPECT_EQ(fd, socket.getSOCKET());
    EXPECT_NO_THROW(socket.close());
}

TEST(SocketImpl, SecondCloseLeavesAReusedDescriptorOpen) {
    SocketImpl first("127.0.0.1", 9);
    first.create();
    const int released = first.getSOCKET();
    first.close();
    // POSIX hands out the lowest free descriptor, so this receives the number
    // just released.
    const int reused = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(reused, 0);
    EXPECT_EQ(released, reused);
    ASSERT_TRUE(isOpen(reused));
    first.close();
    EXPECT_TRUE(isOpen(reused));
    ::close(reused);
}

TEST(SocketImpl, CreateAfterCloseReopens) {
    SocketImpl socket("127.0.0.1", 9);
    socket.create();
    socket.close();
    socket.create();
    ASSERT_TRUE(isOpen(socket.getSOCKET()));
    socket.close();
    EXPECT_FALSE(isOpen(socket.getSOCKET()));
}
