#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>

#include "KernelContext.h"
#include "Properties.h"
#include "SharedServerClient.h"
#include "Socket.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "mofus/MJob.h"
#include "mofus/MPlayer.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
class InspectedMPlayer : public MPlayer {
public:
    using MPlayer::MPlayer;

    bool hasPartialConnection() const {
        const int parts = (m_pSocket != nullptr) + (m_pInputStream != nullptr) + (m_pOutputStream != nullptr);
        return parts != 0 && parts != 3;
    }

    bool hasExpectedBufferSizes() const {
        return m_pInputStream && m_pOutputStream && m_pInputStream->capacity() == 10240 &&
               m_pOutputStream->capacity() == 10240;
    }
};

TEST(GameConnection, MofusPublishesAllConnectionPartsTogetherAndCanRetryAfterAllocationFailure) {
    for (std::size_t failAt = 1; failAt <= 32; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                char directory[] = "/tmp/darkeden-mofus-XXXXXX";
                if (!::mkdtemp(directory) || ::chdir(directory) != 0)
                    std::_Exit(80);
                LoopbackListener listener;
                Properties config;
                config.setProperty("MofusIP", "127.0.0.1");
                config.setProperty("MofusPort", std::to_string(listener.port()));
                de::kernelContext().setConfig(&config);
                MJob job("user", "character", "phone");
                auto player = std::make_unique<InspectedMPlayer>(&job);
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    player->connect();
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                probe.stopFailing();
                const bool complete = !player->hasPartialConnection() && (!threw || !player->getSocket());
                if (complete && !player->getSocket())
                    player->connect();
                const bool connected = player->getSocket() != nullptr && player->hasExpectedBufferSizes();
                player.reset();
                const bool intact = complete && connected && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == available && (failAt != 32 || !probe.rejected());
                ::unlink("mofus_log.txt");
                ::unlink("mofus_error.txt");
                if (::chdir("/") != 0 || ::rmdir(directory) != 0)
                    std::_Exit(81);
                std::_Exit(intact ? 0 : 6);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(GameConnection, SharedClientAdoptionReleasesTheSocketAndPartialStreamsOnEveryAllocationFailure) {
    for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto socket = std::make_unique<Socket>();
                    std::unique_ptr<SharedServerClient> client(new SharedServerClient(socket.release()));
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == available && (failAt != 16 || !probe.rejected());
                std::_Exit(intact ? 0 : 7);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(GameConnection, MofusRejectsInvalidConfiguredPortsBeforeOpeningASocket) {
    for (const char* port : {"0", "65536", "9999junk"}) {
        SCOPED_TRACE(port);
        ASSERT_EXIT(
            {
                Properties config;
                config.setProperty("MofusIP", "127.0.0.1");
                config.setProperty("MofusPort", port);
                de::kernelContext().setConfig(&config);
                MJob job("user", "character", "phone");
                InspectedMPlayer player(&job);
                const int available = nextSocketDescriptor();
                bool rejected = false;
                try {
                    player.connect();
                } catch (const Error& error) {
                    rejected = error.toString().find("MofusPort") != std::string::npos;
                }
                const bool intact = rejected && !player.getSocket() && !player.hasPartialConnection() &&
                                    nextSocketDescriptor() == available;
                std::_Exit(intact ? 0 : 8);
            },
            ::testing::ExitedWithCode(0), "");
    }
}
} // namespace
