#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>

#include "ConnectionInfoManager.h"
#include "GCReconnectLogin.h"
#include "GameConnection.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "KernelContext.h"
#include "Properties.h"
#include "Socket.h"
#include "VariableManager.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
class ListenerConfiguration {
public:
    ListenerConfiguration() {
        std::uint16_t port;
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

private:
    Properties config;
    Properties* previous;
};

TEST(IncomingGameConnection, EmptyManagerDestructionReleasesItsListenerAndAllocations) {
    ASSERT_EXIT(
        {
            ListenerConfiguration config;
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            { IncomingPlayerManager manager; }
            const bool intact = probe.outstanding() == 0 && nextSocketDescriptor() == available;
            std::_Exit(intact ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(IncomingGameConnection, ManagerRestoresItsPreviousConnectionInfoBinding) {
    ASSERT_EXIT(
        {
            ListenerConfiguration config;
            ConnectionInfoManager previous;
            auto& context = de::gameContext();
            context.setConnectionInfoManager(&previous);
            {
                IncomingPlayerManager manager;
                if (&context.connectionInfos() == &previous)
                    std::_Exit(1);
            }
            std::_Exit(&context.connectionInfos() == &previous ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

class CountedReconnectPacket : public GCReconnectLogin {
public:
    explicit CountedReconnectPacket(unsigned& destroyed) : destroyed(destroyed) {}
    ~CountedReconnectPacket() override {
        ++destroyed;
    }

private:
    unsigned& destroyed;
};

class InspectedGamePlayer : public GamePlayer {
public:
    using GamePlayer::GamePlayer;
    bool hasExpectedStreams() const {
        return dynamic_cast<SocketEncryptInputStream*>(m_pInputStream) &&
               dynamic_cast<SocketEncryptOutputStream*>(m_pOutputStream) && m_pInputStream->capacity() == 1024 &&
               m_pOutputStream->capacity() == 20480;
    }
};

class ObservedGamePlayer : public GamePlayer {
public:
    ObservedGamePlayer(Socket* socket, unsigned& ended, unsigned& disconnected, bool failLogout = false)
        : GamePlayer(socket), ended(ended), disconnected(disconnected), failLogout(failLogout) {}
    ~ObservedGamePlayer() noexcept override {
        if (getPlayerStatus() == GPS_END_SESSION)
            ++ended;
    }
    void disconnect(bool) override {
        ++disconnected;
        if (failLogout)
            throw std::bad_alloc();
        setPlayerStatus(GPS_END_SESSION);
    }

private:
    unsigned& ended;
    unsigned& disconnected;
    bool failLogout;
};

TEST(IncomingGameConnection, ClearEmptiesTheTableAndBothQueuesBeforeRepeatedCleanup) {
    ASSERT_EXIT(
        {
            ListenerConfiguration config;
            auto manager = std::make_unique<IncomingPlayerManager>();
            unsigned ended = 0;
            unsigned disconnected = 0;
            for (int index = 0; index < 3; ++index) {
                auto socket = std::make_unique<Socket>();
                auto player =
                    std::unique_ptr<ObservedGamePlayer>(new ObservedGamePlayer(socket.release(), ended, disconnected));
                player->setPlayerStatus(GPS_BEGIN_SESSION);
                if (index == 0)
                    manager->addPlayer(player.get());
                else if (index == 1)
                    manager->pushPlayer(player.get());
                else
                    manager->pushOutPlayer(player.get());
                player.release();
            }
            manager->clearPlayers();
            if (manager->size() != 0 || ended != 3 || disconnected != 3)
                std::_Exit(1);
            manager->clearPlayers();
            manager.reset();
            std::_Exit(ended == 3 && disconnected == 3 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(IncomingGameConnection, FailedPlayerAllocationClosesTheForwardedSocket) {
    ASSERT_EXIT(
        {
            ListenerConfiguration config;
            VariableManager variables;
            de::gameContext().setVariableManager(&variables);
            auto manager = std::make_unique<IncomingPlayerManager>();
            auto info = std::make_unique<ConnectionInfo>();
            info->setClientIP("127.0.0.1");
            de::gameContext().connectionInfos().addConnectionInfo(info.get());
            info.release();
            LoopbackListener listener;
            auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
            socket->connect();
            const int peer = listener.accept();
            const int descriptor = socket->getSOCKET();
            AllocationProbe probe(1);
            manager->acceptNewConnection(socket.release());
            const bool intact = probe.rejected() && probe.outstanding() == 0 && manager->size() == 0 &&
                                nextSocketDescriptor() == descriptor;
            ::close(peer);
            std::_Exit(intact ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(IncomingGameConnection, UnattachedPlayerDestroysReconnectPacketWithoutWorldManagers) {
    ASSERT_EXIT(
        {
            unsigned destroyed = 0;
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            {
                auto socket = std::make_unique<Socket>();
                InspectedGamePlayer player(socket.release());
                if (!player.hasExpectedStreams())
                    std::_Exit(1);
                player.setPlayerStatus(GPS_END_SESSION);
                player.setReconnectPacket(new CountedReconnectPacket(destroyed));
            }
            const bool intact = destroyed == 1 && probe.outstanding() == 0 && nextSocketDescriptor() == available;
            std::_Exit(intact ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(IncomingGameConnection, PlayerConstructionReleasesTheSocketAndEveryPartialMemberOnAllocationFailure) {
    for (std::size_t failAt = 1; failAt <= 32; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto socket = std::make_unique<Socket>();
                    de::GameConnection player(new InspectedGamePlayer(socket.release()));
                    if (!static_cast<InspectedGamePlayer*>(player.get())->hasExpectedStreams() ||
                        player->getPlayerStatus() != GPS_NONE)
                        std::_Exit(1);
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                std::_Exit(threw == probe.rejected() && probe.outstanding() == 0 &&
                                   nextSocketDescriptor() == available && (failAt != 32 || !probe.rejected())
                               ? 0
                               : 2);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(IncomingGameConnection, FailedManagerConstructionKeepsThePreviousBindingAndReleasesItsListener) {
    for (std::size_t failAt = 1; failAt <= 32; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                de::GameContext context;
                ConnectionInfoManager previous;
                context.setConnectionInfoManager(&previous);
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto listener = std::make_unique<ServerSocket>(0);
                    IncomingPlayerManager manager(std::move(listener), context);
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    &context.connectionInfos() == &previous && nextSocketDescriptor() == available &&
                                    (failAt != 32 || !probe.rejected());
                std::_Exit(intact ? 0 : 1);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(IncomingGameConnection, ScopedInjectedManagersRestoreNestedBindingsWithoutPublishingToTheProcessContext) {
    ASSERT_EXIT(
        {
            de::GameContext context;
            ConnectionInfoManager processBinding;
            de::gameContext().setConnectionInfoManager(&processBinding);
            {
                IncomingPlayerManager outer(std::make_unique<ServerSocket>(0), context);
                auto* previous = &context.connectionInfos();
                {
                    IncomingPlayerManager inner(std::make_unique<ServerSocket>(0), context);
                    if (&context.connectionInfos() == previous)
                        std::_Exit(1);
                }
                if (&context.connectionInfos() != previous)
                    std::_Exit(2);
            }
            const bool intact = context.exchangeConnectionInfoManager(nullptr) == nullptr &&
                                &de::gameContext().connectionInfos() == &processBinding;
            std::_Exit(intact ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(IncomingGameConnection, DestructionReleasesTableAndQueuedPlayersOnceButLeavesDetachedPlayersToTheirOwner) {
    ASSERT_EXIT(
        {
            de::GameContext context;
            auto manager = std::make_unique<IncomingPlayerManager>(std::make_unique<ServerSocket>(0), context);
            unsigned ended = 0;
            unsigned disconnected = 0;
            de::GameConnection detached;
            AllocationProbe probe;
            for (int index = 0; index < 4; ++index) {
                auto socket = std::make_unique<Socket>();
                de::GameConnection player(new ObservedGamePlayer(socket.release(), ended, disconnected));
                player->setPlayerStatus(GPS_BEGIN_SESSION);
                if (index == 0) {
                    manager->addPlayer(player.get());
                    // Duplicate bookkeeping references must not become duplicate owners.
                    manager->pushOutPlayer(player.get());
                } else if (index == 1) {
                    manager->pushPlayer(player.get());
                } else if (index == 2) {
                    manager->pushOutPlayer(player.get());
                } else {
                    manager->addPlayer(player.get());
                    manager->deletePlayer(player->getSocket()->getSOCKET());
                    detached = std::move(player);
                }
                player.release();
            }
            manager.reset();
            if (ended != 3 || disconnected != 0 || detached->getPlayerStatus() != GPS_BEGIN_SESSION)
                std::_Exit(1);
            detached.reset();
            std::_Exit(ended == 4 && disconnected == 0 && probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(IncomingGameConnection, ExplicitClearContinuesAfterLogoutFailureAndLeavesNoOwnersForDestruction) {
    ASSERT_EXIT(
        {
            de::GameContext context;
            auto manager = std::make_unique<IncomingPlayerManager>(std::make_unique<ServerSocket>(0), context);
            unsigned ended = 0;
            unsigned disconnected = 0;
            AllocationProbe probe;
            for (int index = 0; index < 3; ++index) {
                auto socket = std::make_unique<Socket>();
                de::GameConnection player(new ObservedGamePlayer(socket.release(), ended, disconnected, index == 0));
                player->setPlayerStatus(GPS_BEGIN_SESSION);
                if (index == 0)
                    manager->addPlayer(player.get());
                else if (index == 1)
                    manager->pushPlayer(player.get());
                else
                    manager->pushOutPlayer(player.get());
                player.release();
            }
            manager->clearPlayers();
            if (manager->size() != 0 || ended != 3 || disconnected != 3 || probe.outstanding() != 0)
                std::_Exit(1);
            manager->clearPlayers();
            manager.reset();
            std::_Exit(ended == 3 && disconnected == 3 && probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}
} // namespace
