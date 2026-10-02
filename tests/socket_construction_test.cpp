#include <unistd.h>

#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include "Player.h"
#include "Socket.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
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
} // namespace
