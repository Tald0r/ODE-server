#include <chrono>
#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>
#include <type_traits>

#include "CGReady.h"
#include "Creature.h"
#include "GCDisconnect.h"
#include "GameConnection.h"
#include "GameContext.h"
#include "IncomingPlayerManager.h"
#include "KernelContext.h"
#include "Properties.h"
#include "Socket.h"
#include "VariableManager.h"
#include "ZonePlayerManager.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"

namespace {
struct PlayerCleanup {
    unsigned destroyed = 0;
    unsigned ended = 0;
    unsigned disconnected = 0;
};

class ObservedPlayer : public GamePlayer {
public:
    ObservedPlayer(Socket* socket, PlayerCleanup& cleanup, bool failLogout)
        : GamePlayer(socket), cleanup(cleanup), failLogout(failLogout) {}
    ~ObservedPlayer() noexcept override {
        ++cleanup.destroyed;
        if (getPlayerStatus() == GPS_END_SESSION)
            ++cleanup.ended;
    }
    void disconnect(bool) override {
        ++cleanup.disconnected;
        if (failLogout)
            throw std::bad_alloc();
        setPlayerStatus(GPS_END_SESSION);
    }

private:
    PlayerCleanup& cleanup;
    bool failLogout;
};

de::GameConnection observe(std::unique_ptr<Socket> socket, PlayerCleanup& cleanup, bool failLogout = false) {
    de::GameConnection player(new ObservedPlayer(socket.release(), cleanup, failLogout));
    player->setPlayerStatus(GPS_BEGIN_SESSION);
    return player;
}

bool closedPeer(int descriptor) {
    pollfd ready{descriptor, POLLIN, 0};
    char byte;
    return ::poll(&ready, 1, 2000) == 1 && ::recv(descriptor, &byte, 1, 0) == 0;
}

template <typename Manager>
bool receivesBroadcast(Manager& manager, const int (&peers)[3], unsigned mask, unsigned sequence = 0) {
    GCDisconnect packet;
    packet.setMessage("handoff");
    SocketOutputStream expected(nullptr, 128);
    for (unsigned previous = 0; previous < sequence; ++previous)
        expected.writePacket(&packet);
    const auto frameStart = expected.length();
    expected.writePacket(&packet);
    manager.broadcastPacket(&packet);
    for (unsigned index = 0; index < 3; ++index) {
        if (!(mask & (1u << index)))
            continue;
        std::string bytes(expected.length() - frameStart, '\0');
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        for (std::size_t offset = 0; offset < bytes.size();) {
            manager.pollSockets();
            manager.processOutputs();
            pollfd ready{peers[index], POLLIN, 0};
            const int result = ::poll(&ready, 1, 10);
            if (result < 0 || std::chrono::steady_clock::now() >= deadline)
                return false;
            if (result == 0)
                continue;
            const auto count = ::recv(peers[index], bytes.data() + offset, bytes.size() - offset, 0);
            if (count <= 0)
                return false;
            offset += count;
        }
        if (bytes != std::string(expected.getBuffer() + frameStart, expected.length() - frameStart))
            return false;
    }
    return true;
}

template <typename Manager> void checkTableHandoff(unsigned moved) {
    de::GameContext context;
    std::unique_ptr<Manager> manager;
    if constexpr (std::is_same_v<Manager, IncomingPlayerManager>)
        manager = std::make_unique<Manager>(std::make_unique<ServerSocket>(0), context);
    else
        manager = std::make_unique<Manager>();
    LoopbackListener listener;
    PlayerCleanup cleanup;
    GamePlayer* players[3];
    int peers[3];
    const int available = nextSocketDescriptor();
    for (unsigned index = 0; index < 3; ++index) {
        auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
        socket->connect();
        socket->setNonBlocking(true);
        peers[index] = listener.accept();
        auto player = observe(std::move(socket), cleanup);
        players[index] = player.get();
        manager->addPlayer(player.get());
        player.release();
    }
    const auto descriptor = players[moved]->getSocket()->getSOCKET();
    AllocationProbe probe(1);
    bool threw = false;
    try {
        manager->moveToOutgoing(players[moved]);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    probe.stopFailing();
    if (!threw || !probe.rejected() || manager->size() != 3 || manager->getPlayer(descriptor) != players[moved] ||
        cleanup.destroyed != 0 || !receivesBroadcast(*manager, peers, 7))
        std::_Exit(1);
    manager->moveToOutgoing(players[moved]);
    if (manager->size() != 2 || cleanup.destroyed != 0 || !receivesBroadcast(*manager, peers, 7 ^ (1u << moved), 1))
        std::_Exit(2);
    pollfd pending{peers[moved], POLLIN, 0};
    if (::poll(&pending, 1, 0) != 0)
        std::_Exit(3);
    manager->clearPlayers();
    if (manager->size() != 0 || cleanup.destroyed != 3 || cleanup.ended != 3 || cleanup.disconnected != 3 ||
        nextSocketDescriptor() != available)
        std::_Exit(4);
    manager->clearPlayers();
    manager.reset();
    for (int peer : peers) {
        if (!closedPeer(peer))
            std::_Exit(5);
        ::close(peer);
    }
    std::_Exit(probe.outstanding() == 0 ? 0 : 6);
}

TEST(GamePlayerHandoff, FailedTableTransfersPreserveSocketServiceAndSuccessfulTransfersUpdateBothManagers) {
    for (unsigned moved = 0; moved < 3; ++moved) {
        SCOPED_TRACE(moved);
        ASSERT_EXIT(checkTableHandoff<IncomingPlayerManager>(moved), ::testing::ExitedWithCode(0), "");
        ASSERT_EXIT(checkTableHandoff<ZonePlayerManager>(moved), ::testing::ExitedWithCode(0), "");
    }
}

TEST(GamePlayerHandoff, KickedIncomingPlayersStayOwnedWhenLogoutThrows) {
    ASSERT_EXIT(
        {
            Properties config;
            de::kernelContext().setConfig(&config);
            VariableManager variables;
            de::gameContext().setVariableManager(&variables);
            de::GameContext context;
            IncomingPlayerManager incoming(std::make_unique<ServerSocket>(0), context);
            PlayerCleanup cleanup;
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            auto player = observe(std::make_unique<Socket>(), cleanup, true);
            player->setPenaltyFlag(PENALTY_TYPE_KICKED);
            incoming.pushPlayer(player.get());
            player.release();
            bool threw = false;
            try {
                incoming.heartbeat();
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            if (!threw || cleanup.destroyed != 1 || cleanup.ended != 1 || cleanup.disconnected != 1 ||
                probe.outstanding() != 0 || nextSocketDescriptor() != available)
                std::_Exit(1);
            incoming.heartbeat();
            incoming.clearPlayers();
            std::_Exit(cleanup.destroyed == 1 && cleanup.disconnected == 1 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

// CGReady only requires an attached creature; world mutation happens later.
// This borrowed fixture deliberately supplies no database or zone graph.
class HandoffCreature : public Creature {
public:
    string toString() const override {
        return name;
    }
    bool load() override {
        return false;
    }
    void save() const override {}
    const string& getName() const override {
        return name;
    }
    CreatureClass getCreatureClass() const override {
        return CREATURE_CLASS_NPC;
    }
    string getCreatureClassString() const override {
        return "handoff";
    }
    Race_t getRace() const override {
        return Race_t{};
    }
    bool isDead() const override {
        return false;
    }
    bool isAlive() const override {
        return true;
    }
    void act(const Timeval&) override {}
    void registerObject() override {}
    Level_t getLevel() const override {
        return 1;
    }

private:
    string name = "handoff";
};

TEST(GamePlayerHandoff, ReadyHandlerRetainsItsRegisteredPlayerWhenOutgoingEnqueueFails) {
    ASSERT_EXIT(
        {
            de::GameContext context;
            IncomingPlayerManager incoming(std::make_unique<ServerSocket>(0), context);
            de::gameContext().setIncomingPlayerManager(&incoming);
            HandoffCreature creature;
            PlayerCleanup cleanup;
            auto owner = observe(std::make_unique<Socket>(), cleanup);
            auto* player = owner.get();
            player->setCreature(&creature);
            player->setPlayerStatus(GPS_WAITING_FOR_CG_READY);
            const int descriptor = player->getSocket()->getSOCKET();
            incoming.addPlayer(owner.get());
            owner.release();
            CGReady ready;
            AllocationProbe probe(1);
            bool threw = false;
            try {
                CGReadyHandler::execute(&ready, player);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            probe.stopFailing();
            if (!threw || !probe.rejected() || incoming.size() != 1 || incoming.getPlayer(descriptor) != player ||
                player->getPlayerStatus() != GPS_WAITING_FOR_CG_READY || cleanup.destroyed != 0)
                std::_Exit(1);
            CGReadyHandler::execute(&ready, player);
            if (incoming.size() != 0 || player->getPlayerStatus() != GPS_NORMAL)
                std::_Exit(2);
            player->setCreature(nullptr);
            incoming.clearPlayers();
            std::_Exit(cleanup.destroyed == 1 && cleanup.ended == 1 && cleanup.disconnected == 1 &&
                               probe.outstanding() == 0
                           ? 0
                           : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GamePlayerHandoff, ZoneQueueRetainsThePlayerWhenIncomingEnqueueFailsAndCanRetry) {
    ASSERT_EXIT(
        {
            de::GameContext context;
            IncomingPlayerManager incoming(std::make_unique<ServerSocket>(0), context);
            de::gameContext().setIncomingPlayerManager(&incoming);
            ZonePlayerManager zone;
            LoopbackListener listener;
            auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
            socket->connect();
            const int peer = listener.accept();
            const int descriptor = socket->getSOCKET();
            PlayerCleanup cleanup;
            auto player = observe(std::move(socket), cleanup);
            zone.pushOutPlayer(player.get());
            player.release();
            AllocationProbe probe(1);
            bool threw = false;
            try {
                zone.heartbeat();
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            probe.stopFailing();
            if (!threw || !probe.rejected() || cleanup.destroyed != 0)
                std::_Exit(1);
            zone.heartbeat();
            incoming.clearPlayers();
            if (cleanup.destroyed != 1 || cleanup.ended != 1 || cleanup.disconnected != 1 || probe.outstanding() != 0 ||
                nextSocketDescriptor() != descriptor || !closedPeer(peer))
                std::_Exit(2);
            ::close(peer);
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GamePlayerHandoff, DuplicateIncomingRegistrationKeepsTheQueuedPlayerForRetry) {
    ASSERT_EXIT(
        {
            char directory[] = "/tmp/darkeden-handoff-XXXXXX";
            if (!::mkdtemp(directory) || ::chdir(directory) != 0)
                std::_Exit(80);
            Properties config;
            de::kernelContext().setConfig(&config);
            de::GameContext context;
            IncomingPlayerManager incoming(std::make_unique<ServerSocket>(0), context);
            PlayerCleanup cleanup;
            auto first = observe(std::make_unique<Socket>(), cleanup);
            auto* registered = first.get();
            const int descriptor = registered->getSocket()->getSOCKET();
            incoming.addPlayer(first.get());
            first.release();
            registered->getSocket()->close();
            auto pending = observe(std::make_unique<Socket>(), cleanup);
            auto* queued = pending.get();
            if (queued->getSocket()->getSOCKET() != descriptor)
                std::_Exit(1);
            incoming.pushPlayer(pending.get());
            pending.release();
            bool threw = false;
            try {
                incoming.heartbeat();
            } catch (const DuplicatedException&) {
                threw = true;
            }
            if (!threw || incoming.size() != 1 || incoming.getPlayer(descriptor) != registered ||
                cleanup.destroyed != 0)
                std::_Exit(2);
            incoming.deletePlayer(descriptor);
            de::GameConnection previous(registered);
            previous.reset();
            incoming.heartbeat();
            if (incoming.size() != 1 || incoming.getPlayer(descriptor) != queued)
                std::_Exit(3);
            incoming.clearPlayers();
            if (cleanup.destroyed != 2 || cleanup.ended != 2 || cleanup.disconnected != 1)
                std::_Exit(4);
            ::unlink("Socket_Error.txt");
            if (::chdir("/") != 0 || ::rmdir(directory) != 0)
                std::_Exit(81);
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GamePlayerHandoff, ZoneDestructionEndsAndReleasesTableAndQueueOwnersWithoutLogout) {
    ASSERT_EXIT(
        {
            PlayerCleanup cleanup;
            const int available = nextSocketDescriptor();
            AllocationProbe probe;
            {
                ZonePlayerManager zone;
                for (int index = 0; index < 3; ++index) {
                    auto player = observe(std::make_unique<Socket>(), cleanup);
                    if (index == 0)
                        zone.addPlayer(player.get());
                    else if (index == 1)
                        zone.pushPlayer(player.get());
                    else
                        zone.pushOutPlayer(player.get());
                    player.release();
                }
            }
            std::_Exit(cleanup.destroyed == 3 && cleanup.ended == 3 && cleanup.disconnected == 0 &&
                               probe.outstanding() == 0 && nextSocketDescriptor() == available
                           ? 0
                           : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(GamePlayerHandoff, ZoneClearEmptiesItsTableAndQueuesBeforeRepeatedCleanup) {
    for (bool failLogout : {false, true}) {
        SCOPED_TRACE(failLogout);
        ASSERT_EXIT(
            {
                auto zone = std::make_unique<ZonePlayerManager>();
                PlayerCleanup cleanup;
                const int available = nextSocketDescriptor();
                AllocationProbe probe;
                for (int index = 0; index < 3; ++index) {
                    auto player = observe(std::make_unique<Socket>(), cleanup, failLogout && index == 0);
                    if (index == 0)
                        zone->addPlayer(player.get());
                    else if (index == 1)
                        zone->pushPlayer(player.get());
                    else
                        zone->pushOutPlayer(player.get());
                    player.release();
                }
                zone->clearPlayers();
                if (zone->size() != 0 || cleanup.destroyed != 3 || cleanup.ended != 3 || cleanup.disconnected != 3 ||
                    probe.outstanding() != 0 || nextSocketDescriptor() != available)
                    std::_Exit(1);
                zone->clearPlayers();
                zone.reset();
                std::_Exit(cleanup.destroyed == 3 && cleanup.ended == 3 && cleanup.disconnected == 3 &&
                                   probe.outstanding() == 0
                               ? 0
                               : 2);
            },
            ::testing::ExitedWithCode(0), "");
    }
}
} // namespace
