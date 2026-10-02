#include <cstdlib>
#include <new>

#include <gtest/gtest.h>

#include "SocketStreams.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"
#include "support/SocketInputStreamTestAccess.h"

namespace {
class SocketStreamSetup : public ::testing::TestWithParam<int> {};

TEST_P(SocketStreamSetup, KeepsTheRequestedSizesAndOmitsUnrequestedStreams) {
    Socket socket;
    const int mask = GetParam();
    auto streams = de::makeSocketStreams(&socket, mask & 1 ? 17 : 0, mask & 2 ? 31 : 0);
    EXPECT_EQ(bool(mask & 1), streams.input != nullptr);
    EXPECT_EQ(bool(mask & 2), streams.output != nullptr);
    if (streams.input) {
        EXPECT_EQ(17u, streams.input->capacity());
        EXPECT_TRUE(streams.input->isEmpty());
        const unsigned char byte = 'i';
        ASSERT_TRUE(SocketInputStreamTestAccess::Preload(*streams.input, &byte, 1));
        char received;
        ASSERT_EQ(1u, streams.input->read(&received, 1));
        EXPECT_EQ('i', received);
    }
    if (streams.output) {
        EXPECT_EQ(31, streams.output->capacity());
        EXPECT_TRUE(streams.output->isEmpty());
        EXPECT_EQ(1u, streams.output->write("o", 1));
        EXPECT_EQ('o', streams.output->getBuffer()[0]);
    }
}

TEST_P(SocketStreamSetup, EveryAllocationFailureReleasesPartialStreamsButKeepsTheBorrowedSocket) {
    const int mask = GetParam();
    for (std::size_t failAt = 1; failAt <= 8; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                Socket socket;
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto streams = de::makeSocketStreams(&socket, mask & 1 ? 17 : 0, mask & 2 ? 31 : 0);
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                int type = 0;
                socklen_t size = sizeof(type);
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    ::getsockopt(socket.getSOCKET(), SOL_SOCKET, SO_TYPE, &type, &size) == 0 &&
                                    nextSocketDescriptor() == available && (failAt != 8 || !probe.rejected());
                std::_Exit(intact ? 0 : 1);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

INSTANTIATE_TEST_SUITE_P(StreamModes, SocketStreamSetup, ::testing::Values(0, 1, 2, 3));

TEST(SocketStreamSetup, NullSocketIsAllowedForEmptyOrOutputOnlySetup) {
    auto empty = de::makeSocketStreams(nullptr, 0, 0);
    EXPECT_EQ(nullptr, empty.input);
    EXPECT_EQ(nullptr, empty.output);
    auto outputOnly = de::makeSocketStreams(nullptr, 0, 23);
    EXPECT_EQ(nullptr, outputOnly.input);
    ASSERT_NE(nullptr, outputOnly.output);
    EXPECT_EQ(23, outputOnly.output->capacity());
    EXPECT_EQ(1u, outputOnly.output->write("x", 1));
    EXPECT_EQ('x', outputOnly.output->getBuffer()[0]);
}

TEST(SocketStreamSetup, InputRequiresASocket) {
    EXPECT_THROW(de::makeSocketStreams(nullptr, 17, 0), AssertionError);
    EXPECT_THROW(de::makeSocketStreams(nullptr, 17, 31), AssertionError);
}
} // namespace
