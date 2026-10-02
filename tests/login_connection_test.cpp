#include <fcntl.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>

#include "KernelContext.h"
#include "LCVersionCheckOK.h"
#include "LoginConnection.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginPlayerManager.h"
#include "PacketDispatcher.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "ReconnectLoginInfoManager.h"
#include "Socket.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
bool readable(int descriptor) {
    pollfd ready{descriptor, POLLIN, 0};
    return ::poll(&ready, 1, 2000) == 1 && (ready.revents & (POLLIN | POLLHUP));
}

bool closedPeer(int descriptor) {
    char byte;
    return readable(descriptor) && ::recv(descriptor, &byte, 1, 0) == 0;
}

bool receivesReply(LoginPlayerManager& manager, int peer, const SocketOutputStream& expected) {
    std::string received(expected.length(), '\0');
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (std::size_t offset = 0; offset < received.size();) {
        manager.pollSockets();
        manager.processOutputs();
        pollfd ready{peer, POLLIN, 0};
        const int result = ::poll(&ready, 1, 10);
        if (result < 0 || std::chrono::steady_clock::now() >= deadline)
            return false;
        if (result == 0)
            continue;
        const auto count = ::recv(peer, received.data() + offset, received.size() - offset, 0);
        if (count <= 0)
            return false;
        offset += count;
    }
    return received == std::string(expected.getBuffer(), expected.length());
}

TEST(LoginConnection, EmptyConnectionHasNoSession) {
    EXPECT_EQ(nullptr, de::makeLoginConnection(nullptr));
}

class InspectedLoginPlayer : public LoginPlayer {
public:
    using LoginPlayer::LoginPlayer;
    bool hasExpectedBuffers() const {
        return m_pInputStream && m_pOutputStream && m_pInputStream->capacity() == 1024 &&
               m_pOutputStream->capacity() == 4096;
    }
};

TEST(LoginConnection, PlayerAdoptionReleasesPartialResourcesAtEveryAllocation) {
    for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                const int available = nextSocketDescriptor();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    auto socket = std::make_unique<Socket>();
                    std::unique_ptr<InspectedLoginPlayer> player(new InspectedLoginPlayer(socket.release()));
                    if (!player->hasExpectedBuffers() || player->getPlayerStatus() != LPS_NONE)
                        std::_Exit(1);
                    player->setPlayerStatus(LPS_END_SESSION);
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    nextSocketDescriptor() == available && (failAt != 16 || !probe.rejected());
                std::_Exit(intact ? 0 : 2);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginConnection, ForwardedAdmissionKeepsOwnershipThroughEveryAllocationAndCanRetry) {
    for (std::size_t failAt = 1; failAt <= 24; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                LoopbackListener listener;
                auto manager = std::make_unique<LoginPlayerManager>();
                auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
                socket->connect();
                const int peer = listener.accept();
                const int descriptor = socket->getSOCKET();
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    manager->acceptNewConnection(socket.release());
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                probe.stopFailing();
                if (threw && (manager->size() != 0 || probe.outstanding() != 0 ||
                              nextSocketDescriptor() != descriptor || !closedPeer(peer)))
                    std::_Exit(1);
                ::close(peer);
                if (threw) {
                    auto retry = std::make_unique<Socket>("127.0.0.1", listener.port());
                    retry->connect();
                    const int retryPeer = listener.accept();
                    manager->acceptNewConnection(retry.release());
                    ::close(retryPeer);
                }
                if (manager->size() != 1 ||
                    static_cast<LoginPlayer*>(manager->getPlayer(descriptor))->getPlayerStatus() != LPS_BEGIN_SESSION)
                    std::_Exit(2);
                manager.reset();
                const bool intact = probe.outstanding() == 0 && nextSocketDescriptor() == descriptor &&
                                    (failAt != 24 || !probe.rejected());
                std::_Exit(intact ? 0 : 3);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginConnection, OccupiedSlotRefusalReleasesTheNewConnectionAndPreservesTheRegisteredPlayer) {
    for (std::size_t failAt = 0; failAt <= 32; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                char directory[] = "/tmp/darkeden-login-refusal-XXXXXX";
                if (!::mkdtemp(directory) || ::chdir(directory) != 0)
                    std::_Exit(80);
                LoopbackListener listener;
                auto manager = std::make_unique<LoginPlayerManager>();
                auto socket = std::make_unique<Socket>();
                auto previous = std::unique_ptr<LoginPlayer>(new LoginPlayer(socket.release()));
                previous->setPlayerStatus(LPS_BEGIN_SESSION);
                auto* registered = previous.get();
                const int descriptor = registered->getSocket()->getSOCKET();
                manager->addPlayer(previous.get());
                previous.release();
                registered->getSocket()->close();
                auto incoming = std::make_unique<Socket>("127.0.0.1", listener.port());
                if (incoming->getSOCKET() != descriptor)
                    std::_Exit(1);
                incoming->connect();
                const int peer = listener.accept();
                AllocationProbe probe(failAt);
                bool refused = false;
                try {
                    manager->acceptNewConnection(incoming.release());
                } catch (const DuplicatedException&) {
                    refused = true;
                } catch (const std::bad_alloc&) {
                    refused = true;
                }
                probe.stopFailing();
                if (!refused || manager->size() != 1 || manager->getPlayer(descriptor) != registered ||
                    registered->getPlayerStatus() != LPS_BEGIN_SESSION || probe.outstanding() != 0 ||
                    nextSocketDescriptor() != descriptor || !closedPeer(peer))
                    std::_Exit(2);
                manager.reset();
                ::close(peer);
                const bool intact = probe.outstanding() == 0 && (failAt != 32 || !probe.rejected()) &&
                                    ::access("assertion_failed.log", F_OK) != 0;
                ::unlink("Socket_Error.txt");
                if (::chdir("/") != 0 || ::rmdir(directory) != 0)
                    std::_Exit(81);
                std::_Exit(intact ? 0 : 3);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

class ObservedLoginPlayer : public LoginPlayer {
public:
    ObservedLoginPlayer(Socket* socket, unsigned& ended, unsigned& disconnected)
        : LoginPlayer(socket), ended(ended), disconnected(disconnected) {}
    ~ObservedLoginPlayer() noexcept override {
        if (getPlayerStatus() == LPS_END_SESSION)
            ++ended;
    }
    void disconnect(bool) override {
        ++disconnected;
    }

private:
    unsigned& ended;
    unsigned& disconnected;
};

TEST(LoginConnection, ManagerTeardownEndsEverySessionWithoutRunningDatabaseLogout) {
    ASSERT_EXIT(
        {
            unsigned ended = 0;
            unsigned disconnected = 0;
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            {
                LoginPlayerManager manager;
                for (int i = 0; i < 2; ++i) {
                    auto socket = std::make_unique<Socket>();
                    auto player = std::unique_ptr<ObservedLoginPlayer>(
                        new ObservedLoginPlayer(socket.release(), ended, disconnected));
                    player->setPlayerStatus(i == 0 ? LPS_BEGIN_SESSION : LPS_PC_MANAGEMENT);
                    player->setID(i == 0 ? "NONE" : "signed-in-account");
                    manager.addPlayer(player.get());
                    player.release();
                }
            }
            const bool intact =
                ended == 2 && disconnected == 0 && probe.outstanding() == 0 && nextSocketDescriptor() == available;
            std::_Exit(intact ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(LoginConnection, ManagerRestoresThePreviousReconnectBinding) {
    ASSERT_EXIT(
        {
            ReconnectLoginInfoManager previous;
            auto& context = de::loginContext();
            context.setReconnectLoginInfoManager(&previous);
            {
                LoginPlayerManager outer;
                auto* outerReconnect = &context.reconnectLogins();
                if (outerReconnect == &previous)
                    std::_Exit(1);
                {
                    LoginPlayerManager inner;
                    if (&context.reconnectLogins() == outerReconnect)
                        std::_Exit(2);
                }
                if (&context.reconnectLogins() != outerReconnect)
                    std::_Exit(3);
            }
            std::_Exit(&context.reconnectLogins() == &previous ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(LoginConnection, FailedManagerConstructionPreservesTheContextAndReleasesItsAllocations) {
    for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                de::LoginContext context;
                ReconnectLoginInfoManager previous;
                context.setReconnectLoginInfoManager(&previous);
                AllocationProbe probe(failAt);
                bool threw = false;
                try {
                    LoginPlayerManager manager(context);
                    if (&context.reconnectLogins() == &previous)
                        std::_Exit(1);
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
                const bool intact = threw == probe.rejected() && probe.outstanding() == 0 &&
                                    &context.reconnectLogins() == &previous && (failAt != 16 || !probe.rejected());
                std::_Exit(intact ? 0 : 2);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginConnection, InjectedContextRestoresAnEmptyBindingWithoutChangingTheProcessContext) {
    ASSERT_EXIT(
        {
            de::LoginContext context;
            ReconnectLoginInfoManager previous;
            de::loginContext().setReconnectLoginInfoManager(&previous);
            {
                LoginPlayerManager manager(context);
                if (&context.reconnectLogins() == &previous || &de::loginContext().reconnectLogins() != &previous)
                    std::_Exit(1);
            }
            std::_Exit(context.exchangeReconnectLoginInfoManager(nullptr) == nullptr &&
                               &de::loginContext().reconnectLogins() == &previous
                           ? 0
                           : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(LoginConnection, PendingPeerErrorsAreQuietlyRefusedAndTheNextConnectionCanBeAdmitted) {
    ASSERT_EXIT(
        {
            LoopbackListener listener;
            LoginPlayerManager manager;
            auto incoming = std::make_unique<Socket>("127.0.0.1", listener.port());
            incoming->connect();
            const int peer = listener.accept();
            const int descriptor = incoming->getSOCKET();
            linger reset{};
            reset.l_onoff = 1;
            if (::setsockopt(peer, SOL_SOCKET, SO_LINGER, &reset, sizeof(reset)) != 0)
                std::_Exit(1);
            ::close(peer);
            if (!readable(descriptor))
                std::_Exit(2);
            manager.acceptNewConnection(incoming.release());
            if (manager.size() != 0 || nextSocketDescriptor() != descriptor)
                std::_Exit(3);
            auto retry = std::make_unique<Socket>("127.0.0.1", listener.port());
            retry->connect();
            const int retryPeer = listener.accept();
            manager.acceptNewConnection(retry.release());
            LCVersionCheckOK packet;
            SocketOutputStream expected(nullptr, 64);
            expected.writePacket(&packet);
            manager.broadcastPacket(&packet);
            const bool intact = manager.size() == 1 && receivesReply(manager, retryPeer, expected);
            ::close(retryPeer);
            std::_Exit(intact ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

class CountedPacket : public LCVersionCheckOK {
public:
    explicit CountedPacket(unsigned& destroyed) : destroyed(destroyed) {}
    ~CountedPacket() override {
        ++destroyed;
    }

private:
    unsigned& destroyed;
};

class CountedPacketFactory : public LCVersionCheckOKFactory {
public:
    explicit CountedPacketFactory(unsigned& destroyed) : destroyed(destroyed) {}
    Packet* createPacket() override {
        return new CountedPacket(destroyed);
    }

private:
    unsigned& destroyed;
};

TEST(LoginConnection, TeardownDeletesPacketHistoryAfterRealInputAndDispatch) {
    ASSERT_EXIT(
        {
            // A child-only factory/validator/handler lets the receive loop
            // retain a counted packet without invoking account persistence.
            // This outbound ID has no production login handler to replace.
            unsigned destroyed = 0;
            PacketFactoryManager factories;
            factories.addFactory(new CountedPacketFactory(destroyed));
            PacketValidator validator;
            auto allowed = std::make_unique<PacketIDSet>(LPS_BEGIN_SESSION);
            allowed->addPacketID(Packet::PACKET_LC_VERSION_CHECK_OK);
            validator.addPacketIDSet(LPS_BEGIN_SESSION, allowed.release());
            de::kernelContext().setPacketFactoryManager(&factories);
            de::kernelContext().setPacketValidator(&validator);
            PacketDispatcher::registerHandler(Packet::PACKET_LC_VERSION_CHECK_OK,
                                              [](Packet* packet, Player* player) { player->sendPacket(packet); });
            LoopbackListener listener;
            auto manager = std::make_unique<LoginPlayerManager>();
            auto incoming = std::make_unique<Socket>("127.0.0.1", listener.port());
            incoming->connect();
            const int peer = listener.accept();
            const int descriptor = incoming->getSOCKET();
            incoming->setNonBlocking(false);
            incoming->setLinger(8);
            manager->acceptNewConnection(incoming.release());
            linger option{};
            socklen_t optionSize = sizeof(option);
            if (!(::fcntl(descriptor, F_GETFL) & O_NONBLOCK) ||
                ::getsockopt(descriptor, SOL_SOCKET, SO_LINGER, &option, &optionSize) != 0 || option.l_onoff != 0)
                std::_Exit(4);
            LCVersionCheckOK packet;
            SocketOutputStream wire(nullptr, 64);
            wire.writePacket(&packet);
            if (::send(peer, wire.getBuffer(), wire.length(), 0) != wire.length() || !readable(descriptor))
                std::_Exit(1);
            manager->pollSockets();
            manager->processInputs();
            manager->processCommands();
            auto* player = static_cast<LoginPlayer*>(manager->getPlayer(descriptor));
            if (!player->getOldPacket(0u) || destroyed != 0 || !receivesReply(*manager, peer, wire))
                std::_Exit(2);
            manager.reset();
            const bool intact = destroyed == 1 && closedPeer(peer);
            ::close(peer);
            std::_Exit(intact ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}
} // namespace
