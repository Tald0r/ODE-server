#include <poll.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "Datagram.h"
#include "DatagramSocket.h"
#include "GameServerInfoManager.h"
#include "GameServerManager.h"
#include "KernelContext.h"
#include "LGKickCharacter.h"
#include "LoginContext.h"
#include "LoginKickDispatch.h"
#include "LoginPlayer.h"
#include "Properties.h"
#include "ServerContext.h"
#include "Socket.h"
#include "repository/ServerInfoRepository.h"
#include "support/AllocationProbe.h"

namespace {

class KickServerRepository : public ServerInfoRepository {
public:
    bool loadMaxServerGroupID(int& value) override {
        value = maxGroup;
        return true;
    }
    bool loadMaxWorldID(int& value) override {
        value = maxWorld;
        return true;
    }
    std::vector<ServerInfoRow> loadServers() override {
        return rows;
    }
    std::vector<ServerInfoNonPKRow> loadNonPKServers() override {
        return {};
    }
    std::vector<ServerInfoCastleStatRow> loadCastleStats() override {
        return {};
    }
    std::vector<ServerInfoWorldRow> loadWorlds() override {
        std::abort();
    }
    int maxWorld = 7;
    int maxGroup = 9;
    std::vector<ServerInfoRow> rows = {{1, "Kick server", "192.0.2.9", 9991, 9992, 7, 9, SERVER_FREE}};
};

class DispatchPlayer : public LoginPlayer {
public:
    DispatchPlayer() : LoginPlayer(new Socket()) {
        setID("account");
        setPlayerStatus(LPS_BEGIN_SESSION);
        cacheLoginKickTarget(target);
    }
    ~DispatchPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
    const de::LoginKickTarget target{7, 9, 3, "Rowan"};
};

TEST(LoginKickDispatch, ASparseWorldReachesItsConfiguredGroup) {
    KickServerRepository repository;
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    unsigned sent = 0;

    de::dispatchLoginKick(player, player.target, servers,
                          [&](const std::string& host, uint port, const LGKickCharacter& packet) {
                              EXPECT_EQ(host, "192.0.2.9");
                              EXPECT_EQ(port, 9992u);
                              EXPECT_EQ(packet.getID(), static_cast<uint>(player.getSocket()->getSOCKET()));
                              EXPECT_EQ(packet.getPCName(), "Rowan");
                              ++sent;
                          });

    EXPECT_EQ(sent, 1u);
    EXPECT_EQ(player.getID(), "account");
    EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
}

TEST(LoginKickDispatch, AMissingLaterFirstServerRefusesBeforeAnySend) {
    KickServerRepository repository;
    repository.maxGroup = 1;
    repository.rows = {{1, "First", "192.0.2.1", 9991, 9992, 7, 0, SERVER_FREE},
                       {2, "No first", "192.0.2.2", 9993, 9994, 7, 1, SERVER_FREE}};
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    unsigned sent = 0;

    de::dispatchLoginKick(player, player.target, servers,
                          [&](const std::string&, uint, const LGKickCharacter&) { ++sent; });

    EXPECT_EQ(sent, 0u);
    EXPECT_EQ(player.getID(), "NONE");
    EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_EQ(player.getExpireTimeForKickCharacter(), (Timeval{}));
}

TEST(LoginKickDispatch, AnEmptyCatalogueDoesNotStartWaitingForAnUnsentPacket) {
    GameServerInfoManager servers;
    DispatchPlayer player;
    unsigned sent = 0;

    de::dispatchLoginKick(player, player.target, servers,
                          [&](const std::string&, uint, const LGKickCharacter&) { ++sent; });

    EXPECT_EQ(sent, 0u);
    EXPECT_EQ(player.getID(), "NONE");
    EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_EQ(player.getExpireTimeForKickCharacter(), (Timeval{}));
}

TEST(LoginKickDispatch, SendsOnlyServerOneInAscendingOccupiedGroupOrder) {
    KickServerRepository repository;
    repository.maxWorld = repository.maxGroup = 255;
    repository.rows = {{1, "Last", "192.0.2.255", 9991, 65535, 7, 255, SERVER_DOWN},
                       {65535, "Another server", "192.0.2.254", 9992, 9993, 7, 255, SERVER_FREE},
                       {1, "Middle", "192.0.2.9", 9994, 9995, 7, 9, SERVER_BUSY},
                       {0, "Zero server", "192.0.2.8", 9996, 9997, 7, 9, SERVER_FREE},
                       {1, "First", "192.0.2.0", 9998, 1, 7, 0, SERVER_NORMAL},
                       {1, "Other world", "192.0.2.1", 9999, 10000, 255, 1, SERVER_FREE}};
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    std::vector<std::string> hosts;
    std::vector<uint> ports;

    de::dispatchLoginKick(player, player.target, servers,
                          [&](const std::string& host, uint port, const LGKickCharacter&) {
                              hosts.push_back(host);
                              ports.push_back(port);
                          });

    EXPECT_EQ(hosts, (std::vector<std::string>{"192.0.2.0", "192.0.2.9", "192.0.2.255"}));
    EXPECT_EQ(ports, (std::vector<uint>{1, 9995, 65535}));
}

TEST(LoginKickDispatch, BothBoundaryWorldsAndGroupsRetainTheirExactDestinations) {
    for (const WorldID_t world : {0, 255}) {
        for (const ServerGroupID_t group : {0, 255}) {
            KickServerRepository repository;
            repository.maxWorld = repository.maxGroup = 255;
            repository.rows = {{1, "Boundary", "192.0.2.1", 1234, 5678, world, group, SERVER_FREE}};
            GameServerInfoManager servers;
            servers.load(repository);
            DispatchPlayer player;
            auto target = player.target;
            target.worldID = world;
            target.groupID = group;
            unsigned sent = 0;

            de::dispatchLoginKick(player, target, servers,
                                  [&](const std::string& host, uint port, const LGKickCharacter&) {
                                      EXPECT_EQ(host, "192.0.2.1");
                                      EXPECT_EQ(port, 5678u);
                                      ++sent;
                                  });

            EXPECT_EQ(sent, 1u);
            EXPECT_EQ(player.getID(), "account");
            EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
        }
    }
}

TEST(LoginKickDispatch, MissingWorldAndLoadedEmptyWorldsRefuseWithoutChangingThePreviousDeadline) {
    for (const bool empty : {false, true}) {
        KickServerRepository repository;
        if (empty)
            repository.rows.clear();
        else
            repository.rows.front().worldID = 1;
        GameServerInfoManager servers;
        servers.load(repository);
        DispatchPlayer player;
        player.setExpireTimeForKickCharacter();
        const auto deadline = player.getExpireTimeForKickCharacter();
        const auto* cached = player.getLoginKickTarget();
        unsigned sent = 0;

        de::dispatchLoginKick(player, player.target, servers,
                              [&](const std::string&, uint, const LGKickCharacter&) { ++sent; });

        EXPECT_EQ(sent, 0u);
        EXPECT_EQ(player.getID(), "NONE");
        EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
        EXPECT_EQ(player.getExpireTimeForKickCharacter(), deadline);
        EXPECT_EQ(player.getLoginKickTarget(), nullptr);
        player.setID("account");
        EXPECT_EQ(player.getLoginKickTarget(), cached);
    }
}

TEST(LoginKickDispatch, TheSavedWorldDrivesDispatchWithoutChangingLiveSelectionOrCache) {
    KickServerRepository repository;
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    player.setWorldID(255);
    player.setServerGroupID(244);
    const auto* cached = player.getLoginKickTarget();
    unsigned sent = 0;

    de::dispatchLoginKick(player, *cached, servers, [&](const std::string& host, uint, const LGKickCharacter&) {
        EXPECT_EQ(host, "192.0.2.9");
        ++sent;
    });

    EXPECT_EQ(sent, 1u);
    EXPECT_EQ(player.getWorldID(), 255);
    EXPECT_EQ(player.getServerGroupID(), 244);
    EXPECT_EQ(player.getLoginKickTarget(), cached);
    EXPECT_EQ(*cached, player.target);
    EXPECT_EQ(player.getKickCharacterCount(), 0);
}

TEST(LoginKickDispatch, AllDestinationsAndPacketValuesAreOwnedBeforeTheFirstSend) {
    KickServerRepository repository;
    repository.rows.push_back({1, "First", "192.0.2.0", 1234, 5678, 7, 0, SERVER_FREE});
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    auto target = player.target;
    std::vector<std::string> hosts;

    de::dispatchLoginKick(player, target, servers, [&](const std::string& host, uint, const LGKickCharacter& packet) {
        if (hosts.empty()) {
            servers.clear();
            target.characterName = "changed character";
        }
        hosts.push_back(host);
        EXPECT_EQ(packet.getPCName(), "Rowan");
        EXPECT_EQ(packet.getID(), static_cast<uint>(player.getSocket()->getSOCKET()));
    });

    EXPECT_EQ(hosts, (std::vector<std::string>{"192.0.2.0", "192.0.2.9"}));
    EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
}

TEST(LoginKickDispatch, WaitingAndItsThreeSecondDeadlineFollowEverySenderReturn) {
    KickServerRepository repository;
    repository.rows.push_back({1, "First", "192.0.2.0", 1234, 5678, 7, 0, SERVER_FREE});
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    Timeval before;
    getCurrentTime(before);
    Timeval lastSend;
    unsigned sent = 0;

    de::dispatchLoginKick(player, player.target, servers, [&](const std::string&, uint, const LGKickCharacter&) {
        EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
        EXPECT_EQ(player.getExpireTimeForKickCharacter(), (Timeval{}));
        EXPECT_EQ(player.getID(), "account");
        getCurrentTime(lastSend);
        ++sent;
    });

    Timeval after;
    getCurrentTime(after);
    before.tv_sec += 3;
    lastSend.tv_sec += 3;
    after.tv_sec += 3;
    EXPECT_EQ(sent, 2u);
    EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
    EXPECT_GE(player.getExpireTimeForKickCharacter(), before);
    EXPECT_GE(player.getExpireTimeForKickCharacter(), lastSend);
    EXPECT_LE(player.getExpireTimeForKickCharacter(), after);
}

TEST(LoginKickDispatch, SenderFailuresRetainExceptionIdentitySessionAndDeadlineAndPermitANewBroadcast) {
    const std::array failures{
        std::make_exception_ptr(std::runtime_error("send failed")), std::make_exception_ptr(Error("send failed")),
        std::make_exception_ptr(NoSuchElementException("send failed")),
        std::make_exception_ptr(DatabaseError("send failed")), std::make_exception_ptr(std::bad_alloc{})};
    for (const unsigned failureStage : {0u, 1u}) {
        for (const auto& failure : failures) {
            KickServerRepository repository;
            repository.rows.push_back({1, "First", "192.0.2.0", 1234, 5678, 7, 0, SERVER_FREE});
            GameServerInfoManager servers;
            servers.load(repository);
            DispatchPlayer player;
            player.setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
            player.setExpireTimeForKickCharacter();
            const auto deadline = player.getExpireTimeForKickCharacter();
            const auto* cached = player.getLoginKickTarget();
            std::vector<std::string> delivered;
            unsigned attempts = 0;
            std::exception_ptr caught;
            try {
                de::dispatchLoginKick(player, player.target, servers,
                                      [&](const std::string& host, uint, const LGKickCharacter&) {
                                          if (attempts++ == failureStage)
                                              std::rethrow_exception(failure);
                                          delivered.push_back(host);
                                      });
            } catch (...) {
                caught = std::current_exception();
            }

            EXPECT_EQ(caught, failure);
            EXPECT_EQ(delivered.size(), failureStage);
            EXPECT_EQ(player.getID(), "account");
            EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
            EXPECT_EQ(player.getExpireTimeForKickCharacter(), deadline);
            EXPECT_EQ(player.getLoginKickTarget(), cached);
            delivered.clear();
            de::dispatchLoginKick(
                player, player.target, servers,
                [&](const std::string& host, uint, const LGKickCharacter&) { delivered.push_back(host); });
            EXPECT_EQ(delivered, (std::vector<std::string>{"192.0.2.0", "192.0.2.9"}));
        }
    }
}

int checkAllocationFailure(std::size_t failAt, bool serialize) {
    KickServerRepository repository;
    repository.rows.front().ip.assign(96, 'i');
    repository.rows.push_back({1, "First", std::string(96, 'j'), 1234, 5678, 7, 0, SERVER_FREE});
    GameServerInfoManager servers;
    servers.load(repository);
    auto player = std::make_unique<DispatchPlayer>();
    auto target = player->target;
    target.characterName.assign(20, 'c');
    auto expectedStatus = player->getPlayerStatus();
    auto expectedDeadline = player->getExpireTimeForKickCharacter();
    const auto* cached = player->getLoginKickTarget();
    unsigned sent = 0;
    bool observedIntact = true;
    const de::LoginKickSend send = [&](const std::string&, uint, const LGKickCharacter& packet) {
        if (player->getPlayerStatus() != expectedStatus ||
            player->getExpireTimeForKickCharacter() != expectedDeadline || player->getID() != "account")
            observedIntact = false;
        if (serialize) {
            Datagram datagram;
            datagram.write(&packet);
        }
        ++sent;
    };
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        de::dispatchLoginKick(*player, target, servers, send);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed) || !observedIntact || probe.outstanding() != 0)
        return 1;
    if (player->getID() != "account" || player->getLoginKickTarget() != cached)
        return 2;
    if (failed) {
        if (player->getPlayerStatus() != expectedStatus ||
            player->getExpireTimeForKickCharacter() != expectedDeadline || (!serialize && sent != 0))
            return 3;
    } else if (sent != 2 || player->getPlayerStatus() != LPS_WAITING_FOR_GL_KICK_VERIFY) {
        return 4;
    }
    expectedStatus = player->getPlayerStatus();
    expectedDeadline = player->getExpireTimeForKickCharacter();
    sent = 0;
    de::dispatchLoginKick(*player, target, servers, send);
    if (sent != 2 || !observedIntact || player->getPlayerStatus() != LPS_WAITING_FOR_GL_KICK_VERIFY)
        return 5;
    player.reset();
    return probe.outstanding() == 0 ? 0 : 6;
}

TEST(LoginKickDispatch, PreparationAndSerializationAllocationFailuresCleanUpAndPermitRetry) {
    for (const bool serialize : {false, true}) {
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(::testing::Message() << serialize << "/" << failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt, serialize)), ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(LoginKickDispatch, ReloadedCataloguesReplaceTheDestinationsForTheNextAttempt) {
    KickServerRepository repository;
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    std::vector<std::string> hosts;
    const de::LoginKickSend send = [&](const std::string& host, uint, const LGKickCharacter&) {
        hosts.push_back(host);
    };
    de::dispatchLoginKick(player, player.target, servers, send);
    repository.rows.front().ip = "192.0.2.10";
    servers.load(repository);

    de::dispatchLoginKick(player, player.target, servers, send);

    EXPECT_EQ(hosts, (std::vector<std::string>{"192.0.2.9", "192.0.2.10"}));
}

TEST(LoginKickDispatch, ARefusedDestinationCanBeRetriedAfterRepairAndReauthentication) {
    KickServerRepository repository;
    repository.rows.front().serverID = 2;
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    unsigned sent = 0;
    const de::LoginKickSend send = [&](const std::string&, uint, const LGKickCharacter&) { ++sent; };
    de::dispatchLoginKick(player, player.target, servers, send);
    EXPECT_EQ(sent, 0u);
    ASSERT_EQ(player.getID(), "NONE");
    repository.rows.front().serverID = 1;
    servers.load(repository);
    player.setID("account");

    de::dispatchLoginKick(player, player.target, servers, send);

    EXPECT_EQ(sent, 1u);
    EXPECT_EQ(player.getID(), "account");
    EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
}

TEST(LoginKickDispatch, SerializationRefusalPreservesTheSessionAndCanRetryWithAValidName) {
    KickServerRepository repository;
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    unsigned sent = 0;
    const de::LoginKickSend send = [&](const std::string&, uint, const LGKickCharacter& packet) {
        Datagram frame;
        frame.write(&packet);
        ++sent;
    };
    auto target = player.target;
    target.characterName.assign(21, 'c');

    EXPECT_THROW(de::dispatchLoginKick(player, target, servers, send), InvalidProtocolException);

    EXPECT_EQ(sent, 0u);
    EXPECT_EQ(player.getID(), "account");
    EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_EQ(player.getExpireTimeForKickCharacter(), (Timeval{}));
    de::dispatchLoginKick(player, player.target, servers, send);
    EXPECT_EQ(sent, 1u);
    EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
}

TEST(LoginKickDispatch, ASuppliedSerializerPreservesTheExistingPacketFieldsAndFrameSize) {
    KickServerRepository repository;
    GameServerInfoManager servers;
    servers.load(repository);
    DispatchPlayer player;
    unsigned sent = 0;

    de::dispatchLoginKick(player, player.target, servers, [&](const std::string&, uint, const LGKickCharacter& packet) {
        Datagram frame;
        frame.write(&packet);
        EXPECT_EQ(frame.getLength(), szPacketHeader + szuint + szBYTE + 5);
        PacketID_t id;
        PacketSize_t size;
        std::memcpy(&id, frame.getData(), szPacketID);
        std::memcpy(&size, frame.getData() + szPacketID, szPacketSize);
        EXPECT_EQ(id, Packet::PACKET_LG_KICK_CHARACTER);
        EXPECT_EQ(size, szuint + szBYTE + 5);
        Datagram body;
        body.setData(frame.getData() + szPacketID + szPacketSize, size);
        LGKickCharacter decoded;
        decoded.read(body);
        EXPECT_EQ(decoded.getID(), static_cast<uint>(player.getSocket()->getSOCKET()));
        EXPECT_EQ(decoded.getPCName(), "Rowan");
        ++sent;
    });

    EXPECT_EQ(sent, 1u);
}

uint boundPort(DatagramSocket& socket) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(socket.getSOCKET(), reinterpret_cast<sockaddr*>(&address), &length) != 0)
        std::abort();
    return ntohs(address.sin_port);
}

int checkProductionDispatch() {
    ::alarm(5);
    DatagramSocket receiver(0);
    uint senderPort;
    {
        DatagramSocket reservation(0);
        senderPort = boundPort(reservation);
    }
    Properties properties;
    properties.setProperty("LoginServerUDPPort", std::to_string(senderPort));
    auto* previousConfig = de::kernelContext().exchangeConfig(&properties);
    GameServerManager sender;
    de::loginContext().setGameServerManager(&sender);
    KickServerRepository repository;
    repository.rows.front().ip = "127.0.0.1";
    repository.rows.front().udpPort = boundPort(receiver);
    GameServerInfoManager servers;
    servers.load(repository);
    de::serverContext().setGameServerInfoManager(&servers);
    DispatchPlayer player;
    player.sendLGKickCharacter();
    pollfd ready{receiver.getSOCKET(), POLLIN, 0};
    if (::poll(&ready, 1, 2000) != 1 || !(ready.revents & POLLIN))
        return 1;
    std::unique_ptr<Datagram> frame(receiver.receive());
    if (!frame || frame->getLength() != szPacketHeader + szuint + szBYTE + 5)
        return 2;
    PacketID_t packetID;
    std::memcpy(&packetID, frame->getData(), szPacketID);
    if (packetID != Packet::PACKET_LG_KICK_CHARACTER)
        return 3;
    Datagram body;
    body.setData(frame->getData() + szPacketID + szPacketSize, szuint + szBYTE + 5);
    LGKickCharacter decoded;
    decoded.read(body);
    if (decoded.getPCName() != "Rowan" || decoded.getID() != static_cast<uint>(player.getSocket()->getSOCKET()) ||
        player.getID() != "account" || player.getPlayerStatus() != LPS_WAITING_FOR_GL_KICK_VERIFY)
        return 4;
    de::serverContext().setGameServerInfoManager(nullptr);
    de::loginContext().setGameServerManager(nullptr);
    de::kernelContext().setConfig(previousConfig);
    return 0;
}

TEST(LoginKickDispatch, TheProductionPlayerAndSenderReachASparseGroupOverLoopbackUDP) {
    ASSERT_EXIT(std::_Exit(checkProductionDispatch()), ::testing::ExitedWithCode(0), "");
}

} // namespace
