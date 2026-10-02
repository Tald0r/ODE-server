#include <fcntl.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <new>
#include <string>

#include <gtest/gtest.h>
#include <sys/resource.h>

#include "GCDisconnect.h"
#include "GameConnection.h"
#include "GameContext.h"
#include "IncomingPlayerManager.h"
#include "Socket.h"
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

std::uint16_t portOf(int descriptor) {
    sockaddr_in address{};
    socklen_t size = sizeof(address);
    if (::getsockname(descriptor, reinterpret_cast<sockaddr*>(&address), &size) != 0)
        std::_Exit(95);
    return ntohs(address.sin_port);
}

void authorize(de::GameContext& context) {
    auto info = std::make_unique<ConnectionInfo>();
    info->setClientIP("127.0.0.1");
    info->setPlayerID("account");
    info->setPCName("character");
    info->setKey(123);
    context.connectionInfos().addConnectionInfo(info.get());
    info.release();
}

bool receivesBroadcast(IncomingPlayerManager& manager, int peer) {
    GCDisconnect packet;
    packet.setMessage("admitted");
    SocketOutputStream expected(nullptr, 128);
    expected.writePacket(&packet);
    manager.broadcast(&packet);
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

class AdmissionLogs {
public:
    AdmissionLogs() {
        if (!::mkdtemp(directory) || ::chdir(directory) != 0)
            std::_Exit(80);
    }
    void remove() {
        for (const char* file :
             {"acceptNewConnection.log", "acceptNewConnectionError.log", "ancNoSuch.log", "ancThrowable.log",
              "ancException.log", "ancEtc.log", "ancDupExcept.log", "Socket_Error.txt"})
            ::unlink(file);
        if (::chdir("/") != 0 || ::rmdir(directory) != 0)
            std::_Exit(81);
    }

private:
    char directory[40] = "/tmp/darkeden-admission-XXXXXX";
};

bool refusalWasLogged(int descriptor) {
    std::ifstream input("ancNoSuch.log");
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return contents.find("127.0.0.1") != std::string::npos &&
           contents.find("FD : " + std::to_string(descriptor)) != std::string::npos;
}

class AdoptedDescriptor : public SocketImpl {
public:
    explicit AdoptedDescriptor(int descriptor) : SocketImpl("127.0.0.1", 0) {
        m_SocketID = descriptor;
    }
};

TEST(GameAdmission, RefusalDiagnosticsCanThrowWithoutLosingTheSocket) {
    ASSERT_EXIT(
        {
            AdmissionLogs logs;
            de::GameContext context;
            unsigned calls = 0;
            IncomingPlayerManager manager(std::make_unique<ServerSocket>(0), context, [&] {
                if (++calls == 2)
                    throw std::bad_alloc();
                return true;
            });
            LoopbackListener listener;
            auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
            socket->connect();
            const int peer = listener.accept();
            const int descriptor = socket->getSOCKET();
            AllocationProbe probe;
            bool threw = false;
            try {
                manager.acceptNewConnection(socket.release());
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            if (!threw || calls != 2 || manager.size() != 0 || probe.outstanding() != 0 || !closedPeer(peer) ||
                nextSocketDescriptor() != descriptor)
                std::_Exit(1);
            ::close(peer);
            logs.remove();
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameAdmission, ZeroAndOutOfTableDescriptorsAreClosedBeforePlayerConstruction) {
    for (bool highDescriptor : {false, true}) {
        SCOPED_TRACE(highDescriptor);
        ASSERT_EXIT(
            {
                de::GameContext context;
                IncomingPlayerManager manager(std::make_unique<ServerSocket>(0), context);
                authorize(context);
                LoopbackListener listener;
                Socket original("127.0.0.1", listener.port());
                original.connect();
                const int peer = listener.accept();
                int descriptor;
                if (highDescriptor) {
                    rlimit limit{};
                    if (::getrlimit(RLIMIT_NOFILE, &limit) != 0 || limit.rlim_max <= PlayerManager::nMaxPlayers)
                        std::_Exit(1);
                    if (limit.rlim_cur <= PlayerManager::nMaxPlayers) {
                        limit.rlim_cur = PlayerManager::nMaxPlayers + 1;
                        if (::setrlimit(RLIMIT_NOFILE, &limit) != 0)
                            std::_Exit(2);
                    }
                    descriptor = ::fcntl(original.getSOCKET(), F_DUPFD, PlayerManager::nMaxPlayers);
                } else {
                    descriptor = ::dup2(original.getSOCKET(), STDIN_FILENO);
                }
                if (descriptor < 0)
                    std::_Exit(3);
                original.close();
                auto impl = std::make_unique<AdoptedDescriptor>(descriptor);
                auto socket = std::unique_ptr<Socket>(new Socket(impl.release()));
                AllocationProbe probe;
                if (!manager.acceptNewConnection(socket.release()) || manager.size() != 0 || probe.outstanding() != 0 ||
                    !closedPeer(peer) || ::fcntl(descriptor, F_GETFD) != -1 || errno != EBADF)
                    std::_Exit(4);
                ::close(peer);
                std::_Exit(0);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(GameAdmission, RefusesUnauthorizedPeersThenAdmitsAfterAuthorizationWithoutDatabaseStartup) {
    ASSERT_EXIT(
        {
            AdmissionLogs logs;
            de::GameContext context;
            bool logging = false;
            auto manager = std::make_unique<IncomingPlayerManager>(std::make_unique<ServerSocket>(0), context,
                                                                   [&] { return logging; });
            LoopbackListener listener;
            for (int attempt = 0; attempt < 3; ++attempt) {
                if (attempt == 2)
                    authorize(context);
                auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
                socket->connect();
                socket->setLinger(8);
                const int peer = listener.accept();
                const int descriptor = socket->getSOCKET();
                if (!manager->acceptNewConnection(socket.release()))
                    std::_Exit(1);
                if (attempt < 2) {
                    if (manager->size() != 0 || !closedPeer(peer) || nextSocketDescriptor() != descriptor)
                        std::_Exit(2);
                    if (attempt == 0 && ::access("ancNoSuch.log", F_OK) == 0)
                        std::_Exit(3);
                    if (attempt == 1 && !refusalWasLogged(descriptor))
                        std::_Exit(4);
                    logging = true;
                } else {
                    auto* player = static_cast<GamePlayer*>(manager->getPlayer(descriptor));
                    linger option{};
                    socklen_t optionSize = sizeof(option);
                    const int flags = ::fcntl(descriptor, F_GETFL);
                    if (manager->size() != 1 || player->getPlayerStatus() != GPS_BEGIN_SESSION || flags < 0 ||
                        !(flags & O_NONBLOCK) ||
                        ::getsockopt(descriptor, SOL_SOCKET, SO_LINGER, &option, &optionSize) != 0 ||
                        option.l_onoff != 0 || !receivesBroadcast(*manager, peer))
                        std::_Exit(5);
                    char byte;
                    if (::send(peer, "q", 1, 0) != 1 || !readable(descriptor) ||
                        player->getSocket()->receive(&byte, 1) != 1 || byte != 'q')
                        std::_Exit(6);
                    manager.reset();
                    if (!closedPeer(peer))
                        std::_Exit(7);
                }
                ::close(peer);
            }
            logs.remove();
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameAdmission, EveryAllocationFailureKeepsAnOwnerThroughConstructionDiagnosticsAndRetry) {
    for (bool logging : {false, true}) {
        SCOPED_TRACE(logging);
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(
                {
                    AdmissionLogs logs;
                    de::GameContext context;
                    auto manager = std::make_unique<IncomingPlayerManager>(std::make_unique<ServerSocket>(0), context,
                                                                           [=] { return logging; });
                    authorize(context);
                    LoopbackListener listener;
                    auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
                    socket->connect();
                    int peer = listener.accept();
                    const int descriptor = socket->getSOCKET();
                    AllocationProbe probe(failAt);
                    try {
                        manager->acceptNewConnection(socket.release());
                    } catch (const std::bad_alloc&) {
                        // Reporting can throw too; its connection must remain owned.
                    }
                    probe.stopFailing();
                    if (manager->size() == 0) {
                        if (probe.outstanding() != 0 || !closedPeer(peer) || nextSocketDescriptor() != descriptor)
                            std::_Exit(1);
                        ::close(peer);
                        auto retry = std::make_unique<Socket>("127.0.0.1", listener.port());
                        retry->connect();
                        peer = listener.accept();
                        manager->acceptNewConnection(retry.release());
                    }
                    if (manager->size() != 1 || !receivesBroadcast(*manager, peer))
                        std::_Exit(2);
                    manager.reset();
                    if (probe.outstanding() != 0 || !closedPeer(peer) || (failAt == 64 && probe.rejected()))
                        std::_Exit(3);
                    ::close(peer);
                    logs.remove();
                    std::_Exit(0);
                },
                ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(GameAdmission, DuplicateRegistrationKeepsTheOldPlayerAndClosesTheRejectedPeerAcrossAllocationFailures) {
    for (std::size_t failAt = 0; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(
            {
                AdmissionLogs logs;
                de::GameContext context;
                auto manager = std::make_unique<IncomingPlayerManager>(std::make_unique<ServerSocket>(0), context,
                                                                       [] { return true; });
                authorize(context);
                LoopbackListener listener;
                auto original = std::make_unique<Socket>();
                auto owner = de::makeGameConnection(std::move(original));
                auto* registered = owner.get();
                const int descriptor = registered->getSocket()->getSOCKET();
                manager->addPlayer(owner.get());
                owner.release();
                registered->getSocket()->close();
                auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
                if (socket->getSOCKET() != descriptor)
                    std::_Exit(1);
                socket->connect();
                const int peer = listener.accept();
                AllocationProbe probe(failAt);
                try {
                    manager->acceptNewConnection(socket.release());
                } catch (const std::bad_alloc&) {
                }
                probe.stopFailing();
                if (manager->size() != 1 || manager->getPlayer(descriptor) != registered ||
                    registered->getPlayerStatus() != GPS_BEGIN_SESSION || probe.outstanding() != 0 ||
                    nextSocketDescriptor() != descriptor || !closedPeer(peer))
                    std::_Exit(2);
                manager->deletePlayer(descriptor);
                de::GameConnection previous(registered);
                previous.reset();
                ::close(peer);
                auto retry = std::make_unique<Socket>("127.0.0.1", listener.port());
                retry->connect();
                const int retryPeer = listener.accept();
                manager->acceptNewConnection(retry.release());
                if (manager->size() != 1 || !receivesBroadcast(*manager, retryPeer))
                    std::_Exit(3);
                manager.reset();
                if (probe.outstanding() != 0 || !closedPeer(retryPeer) || (failAt == 64 && probe.rejected()))
                    std::_Exit(4);
                ::close(retryPeer);
                logs.remove();
                std::_Exit(0);
            },
            ::testing::ExitedWithCode(0), "");
    }
}

TEST(GameAdmission, ListenerAcceptsAndPollsWithoutInitializingTheDatabase) {
    ASSERT_EXIT(
        {
            de::GameContext context;
            auto listener = std::make_unique<ServerSocket>(0);
            const int descriptor = listener->getSOCKET();
            const auto port = portOf(descriptor);
            auto manager = std::make_unique<IncomingPlayerManager>(std::move(listener), context);
            authorize(context);
            if (manager->acceptNewConnection())
                std::_Exit(1);
            Socket peer("127.0.0.1", port);
            peer.connect();
            if (!readable(descriptor))
                std::_Exit(2);
            manager->pollSockets();
            manager->processInputs();
            if (manager->size() != 1 || !receivesBroadcast(*manager, peer.getSOCKET()))
                std::_Exit(3);
            manager.reset();
            std::_Exit(closedPeer(peer.getSOCKET()) ? 0 : 4);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GameAdmission, PendingPeerErrorsAreConsumedWithoutCreatingAPlayer) {
    ASSERT_EXIT(
        {
            de::GameContext context;
            IncomingPlayerManager manager(std::make_unique<ServerSocket>(0), context);
            authorize(context);
            LoopbackListener listener;
            auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
            socket->connect();
            const int peer = listener.accept();
            const int descriptor = socket->getSOCKET();
            linger reset{};
            reset.l_onoff = 1;
            if (::setsockopt(peer, SOL_SOCKET, SO_LINGER, &reset, sizeof(reset)) != 0)
                std::_Exit(1);
            ::close(peer);
            if (!readable(descriptor))
                std::_Exit(2);
            manager.acceptNewConnection(socket.release());
            std::_Exit(manager.size() == 0 && nextSocketDescriptor() == descriptor ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}
} // namespace
