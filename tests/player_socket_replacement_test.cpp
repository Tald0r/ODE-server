#include <cstdlib>
#include <memory>
#include <new>
#include <tuple>

#include <gtest/gtest.h>

#include "Player.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"
#include "support/SocketInputStreamTestAccess.h"

// The same ownership contract is exercised through both public types. The
// shared runtime used to hide Player's setter with a separate noexcept copy.
#if defined(__SHARED_SERVER__)
#include "GameServerPlayer.h"
using ReplacementPlayer = GameServerPlayer;
#else
using ReplacementPlayer = Player;
#endif

namespace {
class InspectedPlayer : public ReplacementPlayer {
public:
    using ReplacementPlayer::ReplacementPlayer;

    SocketInputStream* input() const {
        return m_pInputStream;
    }
    SocketOutputStream* output() const {
        return m_pOutputStream;
    }

    void keepStreams(int mask) {
        if (!(mask & 1)) {
            delete m_pInputStream;
            m_pInputStream = nullptr;
        }
        if (!(mask & 2)) {
            delete m_pOutputStream;
            m_pOutputStream = nullptr;
        }
    }
};

bool openSocket(int descriptor) {
    int type = 0;
    socklen_t size = sizeof(type);
    return ::getsockopt(descriptor, SOL_SOCKET, SO_TYPE, &type, &size) == 0;
}

bool readable(int descriptor) {
    pollfd ready{descriptor, POLLIN, 0};
    return ::poll(&ready, 1, 2000) == 1 && (ready.revents & (POLLIN | POLLHUP));
}

class PlayerSocketReplacement : public ::testing::TestWithParam<std::tuple<int, bool>> {};

TEST_P(PlayerSocketReplacement, AllocationFailurePreservesStateAndReplacementCanBeRetried) {
    const auto [mask, sameSocket] = GetParam();
    for (std::size_t failAt = 1; failAt <= 12; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                auto original = std::make_unique<Socket>();
                auto* originalSocket = original.get();
                const int originalDescriptor = original->getSOCKET();
                std::unique_ptr<InspectedPlayer> player(new InspectedPlayer(original.release()));
                player->keepStreams(mask);
                auto* input = player->input();
                auto* output = player->output();
                const unsigned char byte = 'i';
                if (input && !SocketInputStreamTestAccess::Preload(*input, &byte, 1))
                    std::_Exit(1);
                if (output)
                    output->write("o", 1);
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto replacement = sameSocket ? nullptr : std::make_unique<Socket>();
                    player->setSocket(sameSocket ? originalSocket : replacement.release());
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                probe.stopFailing();
                if (threw) {
                    if (player->getSocket() != originalSocket || player->input() != input ||
                        player->output() != output || !openSocket(originalDescriptor) ||
                        nextSocketDescriptor() != available || probe.outstanding() != 0)
                        std::_Exit(2);
                    char buffered;
                    if (input && (input->length() != 1 || !input->peek(&buffered, 1) || buffered != 'i'))
                        std::_Exit(3);
                    if (output && (output->length() != 1 || output->getBuffer()[0] != 'o'))
                        std::_Exit(4);
                    auto replacement = sameSocket ? nullptr : std::make_unique<Socket>();
                    player->setSocket(sameSocket ? originalSocket : replacement.release());
                }
                if ((player->input() != nullptr) != bool(mask & 1) || (player->output() != nullptr) != bool(mask & 2) ||
                    !openSocket(player->getSocket()->getSOCKET()))
                    std::_Exit(5);
                if ((player->input() &&
                     (player->input()->capacity() != DefaultSocketInputBufferSize || !player->input()->isEmpty())) ||
                    (player->output() &&
                     (player->output()->capacity() != DefaultSocketOutputBufferSize || !player->output()->isEmpty())))
                    std::_Exit(6);
                if (sameSocket ? player->getSocket() != originalSocket : openSocket(originalDescriptor))
                    std::_Exit(7);
                player.reset();
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == originalDescriptor && (failAt != 12 || !probe.rejected());
                std::_Exit(intact ? 0 : 8);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

INSTANTIATE_TEST_SUITE_P(StreamModes, PlayerSocketReplacement,
                         ::testing::Combine(::testing::Values(0, 1, 2, 3), ::testing::Bool()));

TEST(PlayerSocketReplacement, NullInputSocketIsRejectedWithoutDamagingTheConnection) {
    ASSERT_EXIT(
        {
            auto socket = std::make_unique<Socket>();
            auto* original = socket.get();
            std::unique_ptr<InspectedPlayer> player(new InspectedPlayer(socket.release()));
            auto* input = player->input();
            auto* output = player->output();
            bool rejected = false;
            try {
                player->setSocket(nullptr);
            } catch (const AssertionError&) {
                rejected = true;
            }
            const bool intact = rejected && player->getSocket() == original && player->input() == input &&
                                player->output() == output && openSocket(original->getSOCKET());
            if (!intact)
                std::_Exit(1);
            player.reset();
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PlayerSocketReplacement, BarePlayerCanClearItsOwnedSocketWithoutCreatingStreams) {
    ASSERT_EXIT(
        {
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            InspectedPlayer player;
            player.setSocket(new Socket());
            player.setSocket(nullptr);
            player.setSocket(nullptr);
            const bool intact = !player.getSocket() && !player.input() && !player.output() &&
                                probe.outstanding() == 0 && nextSocketDescriptor() == available;
            std::_Exit(intact ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PlayerSocketReplacement, OutputOnlyPlayerCanRetainAMemoryBufferWithNoSocket) {
    ASSERT_EXIT(
        {
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            {
                auto socket = std::make_unique<Socket>();
                InspectedPlayer player(socket.release());
                player.keepStreams(2);
                player.setSocket(nullptr);
                if (player.getSocket() || player.input() || !player.output() || nextSocketDescriptor() != available)
                    std::_Exit(1);
                player.output()->write("x", 1);
                if (player.output()->length() != 1 || player.output()->getBuffer()[0] != 'x')
                    std::_Exit(2);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PlayerSocketReplacement, SuccessfulReplacementClosesTheOldPeerAndUsesTheNewSocketForBothStreams) {
    LoopbackListener listener;
    auto original = std::make_unique<Socket>("127.0.0.1", listener.port());
    original->connect();
    const int oldPeer = listener.accept();
    InspectedPlayer player(original.release());
    auto replacement = std::make_unique<Socket>("127.0.0.1", listener.port());
    replacement->connect();
    const int newPeer = listener.accept();
    player.setSocket(replacement.release());
    ASSERT_TRUE(readable(oldPeer));
    char byte;
    EXPECT_EQ(0, ::recv(oldPeer, &byte, 1, 0));
    player.output()->write("o", 1);
    EXPECT_EQ(1u, player.output()->flush());
    ASSERT_TRUE(readable(newPeer));
    ASSERT_EQ(1, ::recv(newPeer, &byte, 1, 0));
    EXPECT_EQ('o', byte);
    ASSERT_EQ(1, ::send(newPeer, "i", 1, 0));
    ASSERT_TRUE(readable(player.getSocket()->getSOCKET()));
    EXPECT_EQ(1u, player.input()->fill());
    ASSERT_EQ(1u, player.input()->read(&byte, 1));
    EXPECT_EQ('i', byte);
    ::close(oldPeer);
    ::close(newPeer);
}
} // namespace
