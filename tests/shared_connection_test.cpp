#include <fcntl.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>

#include "GameServerManager.h"
#include "KernelContext.h"
#include "Properties.h"
#include "SGGuildInfo.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
class ListenerConfiguration {
public:
    ListenerConfiguration() {
        {
            LoopbackListener available;
            port = available.port();
        }
        config.setProperty("TCPPort", std::to_string(port));
        previous = de::kernelContext().exchangeConfig(&config);
    }
    ~ListenerConfiguration() {
        de::kernelContext().setConfig(previous);
    }
    std::uint16_t port;

private:
    Properties config;
    Properties* previous;
};

bool readable(int fd) {
    pollfd ready{fd, POLLIN, 0};
    return ::poll(&ready, 1, 2000) == 1 && (ready.revents & (POLLIN | POLLHUP));
}

std::uint16_t portOf(ServerSocket& listener) {
    sockaddr_in address{};
    socklen_t size = sizeof(address);
    if (::getsockname(listener.getSOCKET(), reinterpret_cast<sockaddr*>(&address), &size) != 0)
        std::_Exit(95);
    return ntohs(address.sin_port);
}

bool receivesBroadcast(GameServerManager& manager, Socket& peer) {
    SGGuildInfo reply;
    SocketOutputStream expected(nullptr, 128);
    expected.writePacket(&reply);
    manager.broadcast(&reply);
    std::string received(expected.length(), '\0');
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (std::size_t offset = 0; offset < received.size();) {
        manager.pollSockets();
        manager.processOutputs();
        pollfd ready{peer.getSOCKET(), POLLIN, 0};
        const int result = ::poll(&ready, 1, 10);
        if (result < 0 || std::chrono::steady_clock::now() >= deadline)
            return false;
        if (result == 0)
            continue;
        const auto count = ::recv(peer.getSOCKET(), received.data() + offset, received.size() - offset, 0);
        if (count <= 0)
            return false;
        offset += count;
    }
    return received == std::string(expected.getBuffer(), expected.length());
}

TEST(SharedConnection, EmptyManagerDestructionReleasesItsListenerAndAllocations) {
    ASSERT_EXIT(
        {
            ListenerConfiguration config;
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            {
                GameServerManager manager;
                manager.init();
                manager.acceptNewConnection(); // Empty nonblocking accept.
            }
            const bool intact = probe.outstanding() == 0 && nextSocketDescriptor() == available;
            std::_Exit(intact ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedConnection, FailedAcceptAndPlayerAllocationsReleaseEveryPartialConnection) {
    for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                auto listener = std::make_unique<ServerSocket>(0);
                const auto port = portOf(*listener);
                const int listenerDescriptor = listener->getSOCKET();
                auto manager = std::make_unique<GameServerManager>(std::move(listener));
                manager->init();
                Socket peer("127.0.0.1", port);
                peer.connect();
                if (!readable(listenerDescriptor))
                    std::_Exit(3);
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                try {
                    manager->acceptNewConnection();
                } catch (const std::bad_alloc&) {
                    // accept() wrapper allocation propagates; setup failures
                    // are swallowed by the manager. Both must release resources.
                }
                probe.stopFailing();
                if (probe.rejected() && (probe.outstanding() != 0 || nextSocketDescriptor() != available))
                    std::_Exit(1);
                manager.reset();
                const bool intact = probe.outstanding() == 0 && nextSocketDescriptor() == listenerDescriptor &&
                                    (failAt != 16 || !probe.rejected());
                std::_Exit(intact ? 0 : 2);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(SharedConnection, ManagerDestructionClosesEveryAcceptedPeer) {
    ASSERT_EXIT(
        {
            auto listener = std::make_unique<ServerSocket>(0);
            const auto port = portOf(*listener);
            const int listenerDescriptor = listener->getSOCKET();
            auto manager = std::make_unique<GameServerManager>(std::move(listener));
            manager->init();
            Socket first("127.0.0.1", port);
            Socket second("127.0.0.1", port);
            first.connect();
            second.connect();
            AllocationProbe probe;
            if (!readable(listenerDescriptor))
                std::_Exit(3);
            manager->acceptNewConnection();
            if (!readable(listenerDescriptor))
                std::_Exit(4);
            manager->acceptNewConnection();
            manager.reset();
            if (probe.outstanding() != 0 || nextSocketDescriptor() != listenerDescriptor ||
                !readable(first.getSOCKET()) || !readable(second.getSOCKET()))
                std::_Exit(1);
            char byte;
            const bool closed =
                ::recv(first.getSOCKET(), &byte, 1, 0) == 0 && ::recv(second.getSOCKET(), &byte, 1, 0) == 0;
            std::_Exit(closed ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

class CountedPlayer : public GameServerPlayer {
public:
    CountedPlayer(Socket* socket, unsigned& sends, unsigned& destroyed)
        : GameServerPlayer(socket), sends(sends), destroyed(destroyed) {}
    ~CountedPlayer() noexcept override {
        ++destroyed;
    }
    void sendPacket(Packet*) override {
        ++sends;
    }

private:
    unsigned& sends;
    unsigned& destroyed;
};

TEST(SharedConnection, DuplicateRegistrationPreservesBothOwnersWhenADescriptorIsReused) {
    ASSERT_EXIT(
        {
            ListenerConfiguration config;
            auto manager = std::make_unique<GameServerManager>();
            manager->init();
            unsigned sends = 0;
            unsigned destroyed = 0;
            auto socket = std::make_unique<Socket>();
            auto player = std::unique_ptr<CountedPlayer>(new CountedPlayer(socket.release(), sends, destroyed));
            auto* borrowed = player.get();
            manager->addGameServerPlayer(player.get());
            player.release();
            const int descriptor = borrowed->getSocket()->getSOCKET();
            borrowed->getSocket()->close();
            unsigned replacementSends = 0;
            unsigned replacementDestroyed = 0;
            auto replacementSocket = std::make_unique<Socket>();
            if (replacementSocket->getSOCKET() != descriptor)
                std::_Exit(3);
            auto replacement = std::unique_ptr<CountedPlayer>(
                new CountedPlayer(replacementSocket.release(), replacementSends, replacementDestroyed));
            bool rejected = false;
            try {
                manager->addGameServerPlayer(replacement.get());
            } catch (const DuplicatedException&) {
                rejected = true;
            }
            if (!rejected)
                std::_Exit(1);
            manager->broadcast(nullptr);
            manager.reset();
            if (sends != 1 || destroyed != 1 || replacementSends != 0 || replacementDestroyed != 0 ||
                replacement->getSocket()->getSockError())
                std::_Exit(2);
            replacement.reset();
            std::_Exit(replacementDestroyed == 1 ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedConnection, InjectedListenerNeedsNoConfigurationAndRoutesBroadcastsToAcceptedPlayers) {
    ASSERT_EXIT(
        {
            de::kernelContext().setConfig(nullptr);
            auto listener = std::make_unique<ServerSocket>(0);
            const auto port = portOf(*listener);
            const int listenerDescriptor = listener->getSOCKET();
            auto manager = std::make_unique<GameServerManager>(std::move(listener));
            manager->init();
            const int flags = ::fcntl(listenerDescriptor, F_GETFL);
            if (listener || flags < 0 || !(flags & O_NONBLOCK))
                std::_Exit(1);
            manager->acceptNewConnection(); // Empty accept is still harmless.
            Socket peer("127.0.0.1", port);
            peer.connect();
            if (!readable(listenerDescriptor))
                std::_Exit(4);
            manager->acceptNewConnection();
            if (!receivesBroadcast(*manager, peer))
                std::_Exit(2);
            manager.reset();
            char byte;
            const bool closed = readable(peer.getSOCKET()) && ::recv(peer.getSOCKET(), &byte, 1, 0) == 0;
            std::_Exit(closed ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedConnection, EveryConstructionFailureReleasesAnInjectedListenerAndPartialManagerState) {
    for (std::size_t failAt = 1; failAt <= 32; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                de::kernelContext().setConfig(nullptr);
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto listener = std::make_unique<ServerSocket>(0);
                    auto manager = std::make_unique<GameServerManager>(std::move(listener));
                    manager->init();
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == available && (failAt != 32 || !probe.rejected());
                std::_Exit(intact ? 0 : 1);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(SharedConnection, RefusalAndDiagnosticAllocationFailuresCloseOutOfRangeConnectionsAndPermitRetry) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                auto listener = std::make_unique<ServerSocket>(0);
                const auto port = portOf(*listener);
                const int listenerDescriptor = listener->getSOCKET();
                auto manager = std::make_unique<GameServerManager>(std::move(listener));
                manager->init();
                Socket peer("127.0.0.1", port);
                peer.connect();
                if (!readable(listenerDescriptor))
                    std::_Exit(7);
                int padding[GameServerManager::nMaxGameServers];
                unsigned count = 0;
                while (nextSocketDescriptor() < static_cast<int>(GameServerManager::nMaxGameServers)) {
                    if (count == GameServerManager::nMaxGameServers)
                        std::_Exit(1);
                    const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
                    if (descriptor < 0)
                        std::_Exit(2);
                    padding[count++] = descriptor;
                }
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                try {
                    manager->acceptNewConnection();
                } catch (const std::bad_alloc&) {
                }
                probe.stopFailing();
                char byte;
                if (probe.outstanding() != 0 || nextSocketDescriptor() != available || !readable(peer.getSOCKET()) ||
                    ::recv(peer.getSOCKET(), &byte, 1, 0) != 0 || (failAt == 64 && probe.rejected()))
                    std::_Exit(3);
                for (unsigned i = 0; i < count; ++i)
                    ::close(padding[i]);
                {
                    Socket retry("127.0.0.1", port);
                    retry.connect();
                    if (!readable(listenerDescriptor))
                        std::_Exit(8);
                    manager->acceptNewConnection();
                    if (!receivesBroadcast(*manager, retry))
                        std::_Exit(4);
                    manager.reset();
                    if (!readable(retry.getSOCKET()) || ::recv(retry.getSOCKET(), &byte, 1, 0) != 0)
                        std::_Exit(5);
                }
                std::_Exit(probe.outstanding() == 0 ? 0 : 6);
            },
            ::testing::ExitedWithCode(0), "");
    }
}
} // namespace
