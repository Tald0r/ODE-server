#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "BroadcastFilter.h"
#include "Creature.h"
#include "GCDisconnect.h"
#include "GameConnection.h"
#include "SocketOutputStream.h"
#include "ZonePlayerManager.h"
#include "support/LoopbackListener.h"

namespace {
class BroadcastCreature : public Creature {
public:
    explicit BroadcastCreature(Race_t race) : race(race) {}
    std::string toString() const override {
        return "broadcast recipient";
    }
    bool load() override {
        return true;
    }
    void save() const override {}
    const std::string& getName() const override {
        return name;
    }
    CreatureClass getCreatureClass() const override {
        return CREATURE_CLASS_NPC;
    }
    std::string getCreatureClassString() const override {
        return "NPC";
    }
    Race_t getRace() const override {
        return race;
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
    Race_t race;
    std::string name = "recipient";
};

class BorrowingPlayer : public GamePlayer {
public:
    BorrowingPlayer(Socket* socket, Creature& creature) : GamePlayer(socket) {
        setCreature(&creature);
    }
    ~BorrowingPlayer() override {
        // Only the filter needs a creature. The fixture owns it; no world or
        // account teardown is part of this socket-delivery test.
        setCreature(nullptr);
    }
};

std::string frame(const char* message) {
    GCDisconnect packet;
    packet.setMessage(message);
    SocketOutputStream stream(nullptr, 128);
    packet.writeHeaderNBody(stream);
    return {stream.getBuffer(), stream.length()};
}

bool receives(ZonePlayerManager& manager, int peer, const std::string& expected) {
    std::string received(expected.size(), '\0');
    std::size_t offset = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (offset < received.size()) {
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
        offset += static_cast<std::size_t>(count);
    }
    return received == expected;
}

int checkSocketDelivery() {
    BroadcastCreature creatures[] = {BroadcastCreature(RACE_SLAYER), BroadcastCreature(RACE_VAMPIRE),
                                     BroadcastCreature(RACE_SLAYER)};
    LoopbackListener listener;
    auto manager = std::make_unique<ZonePlayerManager>();
    int peers[3]{};
    for (unsigned index = 0; index < 3; ++index) {
        auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
        socket->connect();
        peers[index] = listener.accept();
        socket->setNonBlocking(true);
        de::GameConnection player(new BorrowingPlayer(socket.release(), creatures[index]));
        player->setPlayerStatus(GPS_NORMAL);
        manager->addPlayer(player.get());
        player.release();
    }
    {
        GCDisconnect packet;
        packet.setMessage("everyone");
        manager->pushBroadcastPacket(&packet);
        packet.setMessage("slayers");
        BroadcastFilterRace filter(RACE_SLAYER);
        manager->pushBroadcastPacket(&packet, &filter);
        packet.setMessage("last");
        manager->pushBroadcastPacket(&packet);
        packet.setMessage("never sent");
    }
    manager->heartbeat();
    for (unsigned index = 0; index < 3; ++index) {
        const auto expected = frame("everyone") + (index == 1 ? "" : frame("slayers")) + frame("last");
        if (!receives(*manager, peers[index], expected))
            return 1;
    }
    manager->heartbeat();
    manager->pollSockets();
    manager->processOutputs();
    for (const int peer : peers) {
        pollfd ready{peer, POLLIN, 0};
        if (::poll(&ready, 1, 20) != 0)
            return 2;
    }
    manager.reset();
    for (const int peer : peers) {
        pollfd ready{peer, POLLIN, 0};
        char byte;
        if (::poll(&ready, 1, 2000) != 1 || ::recv(peer, &byte, 1, 0) != 0)
            return 3;
        ::close(peer);
    }
    return 0;
}

TEST(GameBroadcastDelivery, HeartbeatDeliversOwnedFramesInOrderThroughRealRaceFiltersAndSockets) {
    ASSERT_EXIT(std::_Exit(checkSocketDelivery()), ::testing::ExitedWithCode(0), "");
}

struct DeliveryCounts {
    unsigned next[2]{};
};

class ConcurrentRecipient : public GamePlayer {
public:
    ConcurrentRecipient(Socket* socket, DeliveryCounts& counts) : GamePlayer(socket), counts(counts) {}
    void sendStream(SocketOutputStream* stream) override {
        const char* bytes = stream->getBuffer();
        const auto size = stream->length();
        if (size < szPacketHeader + 4 || bytes[0] != static_cast<char>(233) || bytes[1] != 0 || bytes[6] != '0' ||
            static_cast<unsigned char>(bytes[szPacketHeader]) != size - szPacketHeader - 1)
            std::_Exit(80);
        const char* message = bytes + szPacketHeader + 1;
        if ((message[0] != '0' && message[0] != '1') || message[1] != ':')
            std::_Exit(81);
        unsigned sequence = 0;
        const auto parsed = std::from_chars(message + 2, bytes + size, sequence);
        if (parsed.ec != std::errc{} || parsed.ptr != bytes + size || sequence != counts.next[message[0] - '0']++)
            std::_Exit(82);
    }

private:
    DeliveryCounts& counts;
};

int checkConcurrentDelivery() {
    DeliveryCounts counts;
    auto manager = std::make_unique<ZonePlayerManager>();
    auto socket = std::make_unique<Socket>();
    de::GameConnection player(new ConcurrentRecipient(socket.release(), counts));
    player->setPlayerStatus(GPS_NORMAL);
    manager->addPlayer(player.get());
    player.release();
    constexpr unsigned perProducer = 128;
    std::atomic<unsigned> finished = 0;
    const auto produce = [&](unsigned id) {
        for (unsigned sequence = 0; sequence < perProducer; ++sequence) {
            GCDisconnect packet;
            packet.setMessage(std::to_string(id) + ':' + std::to_string(sequence));
            manager->pushBroadcastPacket(&packet);
            std::this_thread::yield();
        }
        ++finished;
    };
    std::jthread first(produce, 0);
    std::jthread second(produce, 1);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (finished.load() != 2) {
        manager->heartbeat();
        if (std::chrono::steady_clock::now() >= deadline)
            std::_Exit(83);
        std::this_thread::yield();
    }
    first.join();
    second.join();
    manager->heartbeat();
    manager.reset();
    return counts.next[0] == perProducer && counts.next[1] == perProducer ? 0 : 1;
}

TEST(GameBroadcastDelivery, HeartbeatConsumesConcurrentProducersOnceAndPreservesEachProducersOrder) {
    ASSERT_EXIT(std::_Exit(checkConcurrentDelivery()), ::testing::ExitedWithCode(0), "");
}
} // namespace
