#include <poll.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "CharacterSelection.h"
#include "DatabaseError.h"
#include "Datagram.h"
#include "DatagramSocket.h"
#include "GLIncomingConnectionOK.h"
#include "GameServerInfoManager.h"
#include "GameServerManager.h"
#include "KernelContext.h"
#include "LCReconnect.h"
#include "LGIncomingConnection.h"
#include "LoginConnection.h"
#include "LoginContext.h"
#include "LoginIncomingReply.h"
#include "LoginIncomingRequest.h"
#include "LoginPlayerManager.h"
#include "Properties.h"
#include "Socket.h"
#include "repository/ServerInfoRepository.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

class Servers : public ServerInfoRepository {
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
        return {};
    }
    std::vector<ServerInfoRow> rows{{3, "Selected server", "192.0.2.9", 9991, 9992, 7, 9, SERVER_FREE}};
    int maxGroup = 9;
    int maxWorld = 7;
};

using Location = std::tuple<int, int, int, std::string>;
using CharacterGroup = std::tuple<int, int, std::string>;

class Accounts : public FakeLoginAccountRepository {
public:
    explicit Accounts(std::vector<std::string>& effects) : effects(effects) {}
    void setCurrentLocation(int world, int group, int slot, const std::string& account) override {
        effects.push_back("account");
        if (beforeWrite)
            beforeWrite();
        if (failure)
            std::rethrow_exception(failure);
        locations.emplace_back(world, group, slot, account);
    }
    std::vector<std::string>& effects;
    std::vector<Location> locations;
    std::function<void()> beforeWrite;
    std::exception_ptr failure;
};

class Characters : public FakeLoginCharacterRepository {
public:
    explicit Characters(std::vector<std::string>& effects) : effects(effects) {}
    void setCharacterServerGroup(WorldID_t world, int group, const std::string& name) override {
        effects.push_back("character");
        if (beforeWrite)
            beforeWrite();
        if (failure)
            std::rethrow_exception(failure);
        groups.emplace_back(world, group, name);
    }
    std::vector<std::string>& effects;
    std::vector<CharacterGroup> groups;
    std::function<void()> beforeWrite;
    std::exception_ptr failure;
};

struct Session {
    explicit Session(const std::string& clientIP = "198.51.100.7")
        : player(de::makeLoginConnection(std::make_unique<Socket>(clientIP, 1234))) {
        servers.load(rows);
        config.setProperty("User", "elca");
        config.setProperty("GameServerUDPPort", "5678");
        player->setID("account");
        player->setWorldID(7);
        player->setServerGroupID(9);
        player->setPlayerStatus(LPS_PC_MANAGEMENT);
        player->setGameServerIP("192.0.2.1");
        player->loginAccountOwnership().acquire("account", [] { return true; });
        request.worldID = 7;
        request.serverGroupID = 9;
        request.playerID = "account";
        request.pcName = "Rowan";
        request.inCharacterManagement = true;
        selected.serverID = 3;
        selected.slot = 2;
    }
    void send() {
        de::requestLoginIncomingConnection(*player, request, selected, services);
    }
    Servers rows;
    GameServerInfoManager servers;
    Properties config;
    std::vector<std::string> effects;
    Accounts accounts{effects};
    Characters characters{effects};
    de::LoginConnection player;
    SelectPCRequest request;
    SelectedCharacter selected;
    unsigned sends = 0;
    de::LoginIncomingRequestServices services{servers,
                                              config,
                                              accounts,
                                              characters,
                                              [&](const std::string&, uint, const LGIncomingConnection&) {
                                                  effects.push_back("send");
                                                  ++sends;
                                              },
                                              {}};
};

void expectUnchanged(const Session& session) {
    EXPECT_EQ(session.player->getPlayerStatus(), LPS_PC_MANAGEMENT);
    EXPECT_EQ(session.player->getGameServerIP(), "192.0.2.1");
    EXPECT_TRUE(session.player->loginAccountOwnership().owns("account"));
    EXPECT_TRUE(session.effects.empty());
    EXPECT_EQ(session.sends, 0u);
}

TEST(LoginIncomingRequest, ACustomDeploymentUserStillSendsToTheConfiguredPort) {
    Session session;
    session.config.setProperty("User", "custom-account");
    session.send();
    EXPECT_EQ(session.sends, 1u);
}

TEST(LoginIncomingRequest, AFailedSendRestoresThePreviousPhaseAndPublicAddress) {
    Session session;
    session.services.send = [](const std::string&, uint, const LGIncomingConnection&) {
        throw std::runtime_error("send failed");
    };
    EXPECT_THROW(session.send(), std::runtime_error);
    EXPECT_EQ(session.player->getPlayerStatus(), LPS_PC_MANAGEMENT);
    EXPECT_EQ(session.player->getGameServerIP(), "192.0.2.1");
}

TEST(LoginIncomingRequest, BrokenDestinationDiagnosticsCannotSkipDispatch) {
    Session session;
    session.config.setProperty("User", "elcastle");
    session.services.reportDestination = [](const std::string&, uint) { throw std::runtime_error("report failed"); };
    EXPECT_NO_THROW(session.send());
    EXPECT_EQ(session.sends, 1u);
}

TEST(LoginIncomingRequest, PortSelectionPreservesTheCatalogueOverrideAndConfiguresEveryOtherUser) {
    for (const auto* user : {"excel96", "beowulf", "crazydog", "elcastle", "elca", "custom", "", "EXCEL96"}) {
        Session session;
        session.config.setProperty("User", user);
        const bool catalogue = std::string(user) == "excel96";
        session.config.setProperty("GameServerUDPPort", catalogue ? "not used" : " +05678 \t\r");
        unsigned reports = 0;
        unsigned sends = 0;
        session.services.send = [&](const std::string& host, uint port, const LGIncomingConnection&) {
            EXPECT_EQ(host, "192.0.2.9");
            EXPECT_EQ(port, catalogue ? 9992u : 5678u);
            ++sends;
        };
        session.services.reportDestination = [&](const std::string& host, uint port) {
            EXPECT_EQ(host, "192.0.2.9");
            EXPECT_EQ(port, 5678u);
            ++reports;
        };
        session.send();
        EXPECT_EQ(sends, 1u);
        EXPECT_EQ(reports, std::string(user) == "elcastle" ? 1u : 0u);
    }
}

TEST(LoginIncomingRequest, ValidatedPortsAndPacketFieldsPrecedeReplyVisiblePublicationAndWrites) {
    Session session;
    session.services.send = [&](const std::string& host, uint port, const LGIncomingConnection& packet) {
        EXPECT_EQ(session.player->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
        EXPECT_EQ(session.player->getGameServerIP(), host);
        EXPECT_EQ(host, "192.0.2.9");
        EXPECT_EQ(port, 5678u);
        EXPECT_TRUE(session.effects.empty());
        EXPECT_TRUE(session.player->loginAccountOwnership().owns("account"));
        Datagram body;
        body.write(&packet);
        ASSERT_EQ(body.getLength(), szPacketHeader + 27u);
        EXPECT_EQ(std::string(body.getData() + szPacketID + szPacketSize, body.getLength() - szPacketHeader),
                  std::string("\x07"
                              "account"
                              "\x05"
                              "Rowan"
                              "\x0c"
                              "198.51.100.7",
                              27));
        session.effects.push_back("send");
    };
    session.send();
    EXPECT_EQ(session.effects, (std::vector<std::string>{"send", "account", "character"}));
    EXPECT_EQ(session.accounts.locations, (std::vector<Location>{{7, 9, 2, "account"}}));
    EXPECT_EQ(session.characters.groups, (std::vector<CharacterGroup>{{7, 9, "Rowan"}}));
    EXPECT_TRUE(session.player->loginAccountOwnership().owns("account"));
}

TEST(LoginIncomingRequest, InvalidPortsAndHostsFailBeforePublicationOrAnyCallback) {
    for (const auto* value : {"", "0", "-1", "65536", "5678junk", "999999999999999999999999"}) {
        Session session;
        session.config.setProperty("GameServerUDPPort", value);
        EXPECT_THROW(session.send(), ConnectException);
        expectUnchanged(session);
    }
    for (const uint value : {0u, 65536u, std::numeric_limits<uint>::max()}) {
        Session session;
        session.config.setProperty("User", "excel96");
        session.servers.getGameServerInfo(3, 9, 7)->setUDPPort(value);
        EXPECT_THROW(session.send(), ConnectException);
        expectUnchanged(session);
    }
    for (const auto& host : {std::string(), std::string("localhost"), std::string("127.1"), std::string("127.000.0.1"),
                             std::string("256.1.2.3"), std::string("127.0.0.1\0x", 11), std::string("::1")}) {
        Session session;
        session.servers.getGameServerInfo(3, 9, 7)->setIP(host);
        EXPECT_THROW(session.send(), ConnectException);
        expectUnchanged(session);
    }
}

TEST(LoginIncomingRequest, MissingConfigurationAndCatalogueRowsPreserveTheSessionForRepair) {
    for (const bool includeUser : {false, true}) {
        Session session;
        Properties missing;
        if (includeUser)
            missing.setProperty("User", "elca");
        const de::LoginIncomingRequestServices services{
            session.servers, missing, session.accounts, session.characters, session.services.send, {}};
        EXPECT_THROW(de::requestLoginIncomingConnection(*session.player, session.request, session.selected, services),
                     NoSuchElementException);
        expectUnchanged(session);
    }
    Session session;
    session.servers.clear();
    EXPECT_THROW(session.send(), NoSuchElementException);
    expectUnchanged(session);
    session.servers.load(session.rows);
    session.send();
    EXPECT_EQ(session.sends, 1u);
}

TEST(LoginIncomingRequest, EveryOtherPhaseAndStaleSelectionAreRefusedBeforeDispatch) {
    for (int value = 0; value < PLAYER_STATUS_MAX; ++value) {
        const auto status = static_cast<PlayerStatus>(value);
        if (status == LPS_PC_MANAGEMENT)
            continue;
        Session session;
        session.player->setPlayerStatus(status);
        EXPECT_THROW(session.send(), DisconnectException);
        EXPECT_EQ(session.player->getPlayerStatus(), status);
        EXPECT_EQ(session.player->getGameServerIP(), "192.0.2.1");
        EXPECT_TRUE(session.effects.empty());
    }
    for (int field = 0; field < 5; ++field) {
        Session session;
        if (field == 0)
            session.request.playerID = "another account";
        if (field == 1)
            session.request.worldID = 1;
        if (field == 2)
            session.request.serverGroupID = 1;
        if (field == 3)
            session.request.inCharacterManagement = false;
        if (field == 4) {
            session.player->setID("NONE");
            session.request.playerID = "NONE";
        }
        EXPECT_THROW(session.send(), DisconnectException);
        expectUnchanged(session);
    }
}

TEST(LoginIncomingRequest, InvalidSlotsAndOversizedPacketFieldsCannotReachTheLegacyNarrowingWriters) {
    for (const int slot : {-1, 0, 4, std::numeric_limits<int>::max()}) {
        Session session;
        session.selected.slot = slot;
        EXPECT_THROW(session.send(), InvalidProtocolException);
        expectUnchanged(session);
    }
    for (const std::size_t length : {0u, 21u, 257u}) {
        for (const bool account : {true, false}) {
            Session session;
            if (account) {
                session.player->setID(std::string(length, 'a'));
                session.request.playerID = std::string(length, 'a');
            } else {
                session.request.pcName = std::string(length, 'c');
            }
            EXPECT_THROW(session.send(), InvalidProtocolException);
            expectUnchanged(session);
        }
    }
    for (const std::size_t length : {0u, 16u, 257u}) {
        Session session(std::string(length, 'i'));
        EXPECT_THROW(session.send(), InvalidProtocolException);
        expectUnchanged(session);
    }
}

TEST(LoginIncomingRequest, BoundaryWorldGroupServerSlotAndPortValuesKeepTheirExactMappings) {
    for (const int boundary : {0, 255}) {
        for (const uint port : {1u, 65535u}) {
            Session session;
            session.rows.maxWorld = session.rows.maxGroup = boundary;
            auto& row = session.rows.rows.front();
            row.worldID = row.groupID = boundary;
            row.serverID = 65535;
            row.udpPort = port;
            session.servers.load(session.rows);
            session.player->setWorldID(boundary);
            session.player->setServerGroupID(boundary);
            session.request.worldID = session.request.serverGroupID = boundary;
            session.selected.serverID = 65535;
            session.selected.slot = boundary == 0 ? 1 : 3;
            session.config.setProperty("User", "excel96");
            session.services.send = [&](const std::string&, uint observed, const LGIncomingConnection&) {
                EXPECT_EQ(observed, port);
            };
            session.send();
            EXPECT_EQ(session.accounts.locations,
                      (std::vector<Location>{{boundary, boundary, boundary == 0 ? 1 : 3, "account"}}));
            EXPECT_EQ(session.characters.groups, (std::vector<CharacterGroup>{{boundary, boundary, "Rowan"}}));
        }
    }
}

TEST(LoginIncomingRequest, PreparedValuesOutliveCatalogueReloadAndChangesToBorrowedRequestInputs) {
    Session session;
    session.config.setProperty("User", "elcastle");
    session.services.reportDestination = [&](const std::string&, uint) {
        EXPECT_EQ(session.player->getPlayerStatus(), LPS_PC_MANAGEMENT);
        session.servers.clear();
        session.request.playerID = "changed";
        session.request.pcName = "changed character";
        session.request.worldID = 1;
        session.request.serverGroupID = 2;
        session.selected.slot = 3;
        session.config.setProperty("GameServerUDPPort", "1234");
    };
    session.services.send = [&](const std::string& host, uint port, const LGIncomingConnection& packet) {
        EXPECT_EQ(host, "192.0.2.9");
        EXPECT_EQ(port, 5678u);
        EXPECT_EQ(packet.getPlayerID(), "account");
        EXPECT_EQ(packet.getPCName(), "Rowan");
        EXPECT_EQ(packet.getClientIP(), "198.51.100.7");
        session.effects.push_back("send");
    };
    session.accounts.beforeWrite = [&] { session.request.pcName = "changed again"; };
    session.send();
    EXPECT_EQ(session.effects, (std::vector<std::string>{"send", "account", "character"}));
    EXPECT_EQ(session.accounts.locations, (std::vector<Location>{{7, 9, 2, "account"}}));
    EXPECT_EQ(session.characters.groups, (std::vector<CharacterGroup>{{7, 9, "Rowan"}}));
}

const auto failures = std::array{std::make_exception_ptr(Error("operation failed")),
                                 std::make_exception_ptr(NoSuchElementException("operation failed")),
                                 std::make_exception_ptr(DatabaseError("operation failed")),
                                 std::make_exception_ptr(std::runtime_error("operation failed")),
                                 std::make_exception_ptr(std::bad_alloc{})};

TEST(LoginIncomingRequest, SendFailuresRestoreOwnedAddressStorageAndKeepTheOriginalCauseAndAccount) {
    for (const auto& failure : failures) {
        Session session;
        const std::string previous(96, 'p');
        session.player->setGameServerIP(previous);
        const auto* borrowedAddress = session.player->getGameServerIP().data();
        const auto* borrowedAccount = session.player->loginAccountOwnership().account();
        session.services.send = [&](const std::string&, uint, const LGIncomingConnection&) {
            std::rethrow_exception(failure);
        };
        std::exception_ptr caught;
        try {
            session.send();
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(session.player->getPlayerStatus(), LPS_PC_MANAGEMENT);
        EXPECT_EQ(session.player->getGameServerIP(), previous);
        EXPECT_EQ(session.player->getGameServerIP().data(), borrowedAddress);
        EXPECT_EQ(session.player->loginAccountOwnership().account(), borrowedAccount);
        EXPECT_TRUE(session.effects.empty());
        session.services.send = [](const std::string&, uint, const LGIncomingConnection&) {};
        EXPECT_NO_THROW(session.send());
        EXPECT_EQ(session.accounts.locations.size(), 1u);
    }
}

TEST(LoginIncomingRequest, PersistenceFailuresKeepTheSentRequestAndNeverRepeatEarlierWrites) {
    for (const bool firstWrite : {true, false}) {
        for (const auto& failure : failures) {
            Session session;
            if (firstWrite)
                session.accounts.failure = failure;
            else
                session.characters.failure = failure;
            std::exception_ptr caught;
            try {
                session.send();
            } catch (...) {
                caught = std::current_exception();
            }
            EXPECT_EQ(caught, failure);
            EXPECT_EQ(session.sends, 1u);
            EXPECT_EQ(session.player->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
            EXPECT_EQ(session.player->getGameServerIP(), "192.0.2.9");
            EXPECT_TRUE(session.player->loginAccountOwnership().owns("account"));
            EXPECT_EQ(session.accounts.locations.size(), firstWrite ? 0u : 1u);
            EXPECT_TRUE(session.characters.groups.empty());
            EXPECT_EQ(session.effects, firstWrite ? (std::vector<std::string>{"send", "account"})
                                                  : (std::vector<std::string>{"send", "account", "character"}));
            EXPECT_THROW(session.send(), DisconnectException);
            EXPECT_EQ(session.sends, 1u);
        }
    }
}

TEST(LoginIncomingRequest, DiagnosticsCannotChangeTheResultEvenForNonThrowableFailures) {
    for (const auto& failure : failures) {
        Session session;
        session.config.setProperty("User", "elcastle");
        session.services.reportDestination = [&](const std::string&, uint) { std::rethrow_exception(failure); };
        EXPECT_NO_THROW(session.send());
        EXPECT_EQ(session.effects, (std::vector<std::string>{"send", "account", "character"}));
        EXPECT_EQ(session.player->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
    }
}

TEST(LoginIncomingRequest, TheAcceptedSelectionDecisionFeedsTheRequestForEveryRace) {
    class Topology : public SelectPCTopology {
        bool isNonPKServer(WorldID_t, ServerGroupID_t) override {
            return false;
        }
        ServerID_t zoneServerID(ZoneID_t) override {
            return 3;
        }
    } topology;
    for (const PCType race : {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS}) {
        Session session;
        session.request.pcType = race;
        const auto table = race == PC_SLAYER    ? LOGIN_RACE_TABLE_SLAYER
                           : race == PC_VAMPIRE ? LOGIN_RACE_TABLE_VAMPIRE
                                                : LOGIN_RACE_TABLE_OUSTERS;
        session.characters.addSelectableCharacter(table, "account", "Rowan", 11, "SLOT2", 1, 1);
        auto outcome = decideSelectPC(session.request, session.characters, topology);
        ASSERT_FALSE(outcome.isRejected());
        session.selected = std::move(outcome).events();
        session.send();
        EXPECT_EQ(session.accounts.locations, (std::vector<Location>{{7, 9, 2, "account"}}));
        EXPECT_EQ(session.characters.groups, (std::vector<CharacterGroup>{{7, 9, "Rowan"}}));
    }
}

TEST(LoginIncomingRequest, ThePublishedRequestCanCompleteThroughTheReplyFlowUnderTheManagerLock) {
    Session session;
    de::LoginContext context;
    LoginPlayerManager players(context);
    auto* player = session.player.get();
    players.addPlayer(player);
    session.player.release();
    session.services.send = [&](const std::string&, uint, const LGIncomingConnection&) {
        EXPECT_THROW(players.lock(), Error);
        EXPECT_EQ(player->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
        EXPECT_EQ(player->getGameServerIP(), "192.0.2.9");
    };
    {
        std::lock_guard guard(players);
        de::requestLoginIncomingConnection(*player, session.request, session.selected, session.services);
    }
    GLIncomingConnectionOK reply;
    reply.setPlayerID("account");
    reply.setTCPPort(9991);
    reply.setKey(0x12345678);
    unsigned replies = 0;
    const de::LoginIncomingReplyActions actions{[&](LoginPlayer&, LCReconnect& packet) {
                                                    EXPECT_THROW(players.lock(), Error);
                                                    EXPECT_EQ(packet.getGameServerIP(), "192.0.2.9");
                                                    EXPECT_EQ(packet.getGameServerPort(), 9991u);
                                                    EXPECT_EQ(packet.getKey(), 0x12345678u);
                                                    ++replies;
                                                },
                                                {[](LoginPlayer& retired, bool flush) {
                                                     EXPECT_TRUE(flush);
                                                     retired.loginAccountOwnership().release([](const std::string&) {});
                                                 },
                                                 {}}};
    EXPECT_TRUE(de::completeLoginIncomingConnection(players, reply, {}, actions));
    EXPECT_EQ(replies, 1u);
    EXPECT_EQ(players.size(), 0u);
    EXPECT_EQ(players.pendingRetirements(), 0u);
}

uint boundPort(DatagramSocket& socket) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(socket.getSOCKET(), reinterpret_cast<sockaddr*>(&address), &length) != 0)
        std::_Exit(90);
    return ntohs(address.sin_port);
}

int checkProductionSender() {
    ::alarm(5);
    Session session;
    DatagramSocket receiver(0);
    uint senderPort;
    {
        DatagramSocket reservation(0);
        senderPort = boundPort(reservation);
    }
    session.config.setProperty("User", "custom-deployment");
    session.config.setProperty("LoginServerUDPPort", std::to_string(senderPort));
    session.config.setProperty("GameServerUDPPort", std::to_string(boundPort(receiver)));
    auto* previousConfig = de::kernelContext().exchangeConfig(&session.config);
    GameServerManager sender;
    session.servers.getGameServerInfo(3, 9, 7)->setIP("127.0.0.1");
    session.services.send = [&](const std::string& host, uint port, const LGIncomingConnection& packet) {
        sender.sendPacket(host, port, &packet);
    };
    session.send();
    pollfd ready{receiver.getSOCKET(), POLLIN, 0};
    if (::poll(&ready, 1, 2000) != 1 || !(ready.revents & POLLIN))
        return 1;
    std::unique_ptr<Datagram> frame(receiver.receive());
    if (!frame || frame->getLength() != szPacketHeader + 27)
        return 2;
    PacketID_t packetID;
    std::memcpy(&packetID, frame->getData(), szPacketID);
    if (packetID != Packet::PACKET_LG_INCOMING_CONNECTION)
        return 3;
    Datagram body;
    body.setData(frame->getData() + szPacketID + szPacketSize, 27);
    LGIncomingConnection packet;
    packet.read(body);
    const bool intact = packet.getPlayerID() == "account" && packet.getPCName() == "Rowan" &&
                        packet.getClientIP() == "198.51.100.7" &&
                        session.player->getPlayerStatus() == LPS_AFTER_SENDING_LG_INCOMING_CONNECTION &&
                        session.accounts.locations.size() == 1 && session.characters.groups.size() == 1;
    de::kernelContext().setConfig(previousConfig);
    return intact ? 0 : 4;
}

TEST(LoginIncomingRequest, TheProductionSenderDeliversThePreparedRequestOverLoopbackUDP) {
    ASSERT_EXIT(std::_Exit(checkProductionSender()), ::testing::ExitedWithCode(0), "");
}

int checkAllocationFailure(std::size_t failAt, bool serialize) {
    auto session = std::make_unique<Session>();
    session->config.setProperty("User", std::string(96, 'u'));
    session->config.setProperty("GameServerUDPPort", std::string(96, ' ') + "5678");
    const std::string previous(96, 'p');
    session->player->setGameServerIP(previous);
    const auto* borrowed = session->player->getGameServerIP().data();
    bool sent = false;
    session->services.send = [&](const std::string& host, uint port, const LGIncomingConnection& packet) {
        if (session->player->getPlayerStatus() != LPS_AFTER_SENDING_LG_INCOMING_CONNECTION ||
            session->player->getGameServerIP() != host || host != "192.0.2.9" || port != 5678)
            std::_Exit(7);
        if (serialize) {
            Datagram frame;
            frame.write(&packet);
        }
        sent = true;
    };
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        session->send();
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 48 && failed) ||
        !session->player->loginAccountOwnership().owns("account"))
        return 1;
    if (failed && !sent) {
        if (session->player->getPlayerStatus() != LPS_PC_MANAGEMENT || session->player->getGameServerIP() != previous ||
            session->player->getGameServerIP().data() != borrowed || !session->effects.empty() ||
            probe.outstanding() != 0)
            return 2;
        session->send();
        if (session->accounts.locations.size() != 1 || session->characters.groups.size() != 1)
            return 3;
    } else if (session->player->getPlayerStatus() != LPS_AFTER_SENDING_LG_INCOMING_CONNECTION ||
               session->player->getGameServerIP() != "192.0.2.9" || !sent) {
        return 4;
    }
    session.reset();
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(LoginIncomingRequest, AllocationFailuresPreservePreSendRetryOrTheAcknowledgedRequestAndReleaseAllStorage) {
    for (const bool serialize : {false, true}) {
        for (std::size_t failAt = 1; failAt <= 48; ++failAt) {
            SCOPED_TRACE(::testing::Message() << serialize << "/" << failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt, serialize)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
