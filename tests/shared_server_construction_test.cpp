#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#include <gtest/gtest.h>

#include "GameServerGroupInfoManager.h"
#include "GameServerManager.h"
#include "GuildManager.h"
#include "KernelContext.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "Properties.h"
#include "ServerContext.h"
#include "ServerSocket.h"
#include "SharedContext.h"
#include "SharedGameServerInfoManager.h"
#include "SharedServer.h"
#include "database/DatabaseManager.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {

TEST(SharedServerConstruction, ListenerSetupFailureReleasesTheConstructedManagers) {
    ASSERT_EXIT(
        {
            ::alarm(3);
            Properties config;
            config.setProperty("TCPPort", "0");
            de::kernelContext().setConfig(&config);
            (void)de::serverContext();
            (void)de::sharedContext();
            AllocationProbe probe;
            bool refused = false;
            try {
                SharedServer server;
            } catch (const Error&) {
                refused = true;
            }
            std::_Exit(refused && probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedServerConstruction, ListenerSetupFailureRestoresPreviousContextBindings) {
    ASSERT_EXIT(
        {
            ::alarm(3);
            Properties config;
            config.setProperty("TCPPort", "0");
            DatabaseManager database;
            GuildManager guilds;
            PacketFactoryManager factories;
            PacketValidator validator;
            auto& kernel = de::kernelContext();
            auto& serverContext = de::serverContext();
            auto& shared = de::sharedContext();
            kernel.setConfig(&config);
            kernel.setPacketFactoryManager(&factories);
            kernel.setPacketValidator(&validator);
            serverContext.setDatabaseManager(&database);
            shared.setGuildManager(&guilds);
            bool refused = false;
            try {
                SharedServer server;
            } catch (const Error&) {
                refused = true;
            }
            const bool restored = &kernel.config() == &config && &kernel.packetFactories() == &factories &&
                                  &kernel.packetValidator() == &validator && &serverContext.database() == &database &&
                                  &shared.guilds() == &guilds;
            std::_Exit(refused && restored ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedServerConstruction, ServerInfoManagerHasEmptyStateBeforeInitialization) {
    ASSERT_EXIT(
        {
            alignas(SharedGameServerInfoManager) unsigned char storage[sizeof(SharedGameServerInfoManager)];
            std::memset(storage, 0xa5, sizeof(storage));
            auto* manager = new (storage) SharedGameServerInfoManager;
            if (manager->getMaxServerGroupID() != 0)
                std::_Exit(1);
            manager->~SharedGameServerInfoManager();
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(SharedServerConstruction, ServerGroupInfoManagerCanBeDestroyedBeforeInitialization) {
    ASSERT_EXIT(
        {
            ::alarm(3);
            alignas(GameServerGroupInfoManager) unsigned char storage[sizeof(GameServerGroupInfoManager)];
            std::memset(storage, 0xa5, sizeof(storage));
            auto* manager = new (storage) GameServerGroupInfoManager;
            manager->~GameServerGroupInfoManager();
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

using Bindings = std::array<const void*, 7>;

Bindings currentBindings() {
    return {&de::serverContext().database(),        &de::serverContext().worldInfos(),
            &de::kernelContext().packetFactories(), &de::kernelContext().packetValidator(),
            &de::sharedContext().guilds(),          &de::sharedContext().gameServers(),
            &de::sharedContext().strings()};
}

Bindings clearBindings() {
    return {de::serverContext().exchangeDatabaseManager(nullptr),
            de::serverContext().exchangeGameWorldInfoManager(nullptr),
            de::kernelContext().exchangePacketFactoryManager(nullptr),
            de::kernelContext().exchangePacketValidator(nullptr),
            de::sharedContext().exchangeGuildManager(nullptr),
            de::sharedContext().exchangeGameServerManager(nullptr),
            de::sharedContext().exchangeStringPool(nullptr)};
}

bool allBindingsChanged(const Bindings& previous) {
    const auto current = currentBindings();
    for (std::size_t i = 0; i < current.size(); ++i)
        if (current[i] == previous[i])
            return false;
    return true;
}

bool closed(int descriptor) {
    return ::fcntl(descriptor, F_GETFD) == -1 && errno == EBADF;
}

int checkNestedScopes() {
    (void)clearBindings();
    Properties config;
    de::kernelContext().setConfig(&config);
    auto outerListener = std::make_unique<ServerSocket>(0);
    const int outerDescriptor = outerListener->getSOCKET();
    {
        SharedServer outer(std::move(outerListener));
        const auto outerBindings = currentBindings();
        auto innerListener = std::make_unique<ServerSocket>(0);
        const int innerDescriptor = innerListener->getSOCKET();
        {
            SharedServer inner(std::move(innerListener));
            if (!allBindingsChanged(outerBindings) || closed(innerDescriptor) ||
                &de::kernelContext().config() != &config)
                return 1;
        }
        if (currentBindings() != outerBindings || !closed(innerDescriptor) || closed(outerDescriptor))
            return 2;
    }
    return clearBindings() == Bindings{} && closed(outerDescriptor) && &de::kernelContext().config() == &config ? 0 : 3;
}

TEST(SharedServerConstruction, NestedScopesRestoreEveryBindingAndCloseOnlyTheirOwnedListeners) {
    ASSERT_EXIT(std::_Exit(checkNestedScopes()), ::testing::ExitedWithCode(0), "");
}

int checkAllocationFailure(std::size_t failAt) {
    // Neither construction path needs database initialization or configuration
    // when given an already bound listener. The previous graph remains usable.
    de::kernelContext().setConfig(nullptr);
    SharedServer previous(std::make_unique<ServerSocket>(0));
    const auto previousBindings = currentBindings();
    auto listener = std::make_unique<ServerSocket>(0);
    const int descriptor = listener->getSOCKET();
    AllocationProbe probe(failAt);
    bool failed = false;
    bool published = false;
    try {
        SharedServer candidate(std::move(listener));
        published = allBindingsChanged(previousBindings);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (!failed && !published) || (failAt == 64 && probe.rejected()))
        return 1;
    if (listener || !closed(descriptor) || currentBindings() != previousBindings || probe.outstanding() != 0)
        return 2;
    if (failed) {
        {
            SharedServer retry(std::make_unique<ServerSocket>(0));
            if (!allBindingsChanged(previousBindings))
                return 3;
        }
        if (currentBindings() != previousBindings || probe.outstanding() != 0)
            return 4;
    }
    return 0;
}

TEST(SharedServerConstruction, AllocationFailuresReleaseIncomingOwnershipPreserveBindingsAndPermitRetry) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

int checkConnectionCleanup() {
    (void)clearBindings();
    de::kernelContext().setConfig(nullptr);
    auto listener = std::make_unique<ServerSocket>(0);
    const int listenerDescriptor = listener->getSOCKET();
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(listenerDescriptor, reinterpret_cast<sockaddr*>(&address), &length) != 0)
        return 90;
    Socket peer("127.0.0.1", ntohs(address.sin_port));
    int acceptedDescriptor = -1;
    AllocationProbe probe;
    {
        SharedServer server(std::move(listener));
        auto& manager = de::sharedContext().gameServers();
        manager.init(); // Only the socket poll set; no database or worker startup.
        peer.connect();
        pollfd ready{listenerDescriptor, POLLIN, 0};
        if (::poll(&ready, 1, 2000) != 1)
            return 1;
        acceptedDescriptor = nextSocketDescriptor();
        manager.acceptNewConnection();
        if (closed(acceptedDescriptor))
            return 2;
    }
    if (probe.outstanding() != 0 || !closed(listenerDescriptor) || !closed(acceptedDescriptor) ||
        clearBindings() != Bindings{})
        return 3;
    pollfd ready{peer.getSOCKET(), POLLIN, 0};
    char byte;
    return ::poll(&ready, 1, 2000) == 1 && ::recv(peer.getSOCKET(), &byte, 1, 0) == 0 ? 0 : 4;
}

TEST(SharedServerConstruction, QuiescentDestructionReleasesTheListenerPlayersAndTheirAllocations) {
    ASSERT_EXIT(
        {
            ::alarm(5);
            std::_Exit(checkConnectionCleanup());
        },
        ::testing::ExitedWithCode(0), "");
}

} // namespace
