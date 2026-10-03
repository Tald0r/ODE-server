#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <streambuf>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "CLSelectPC.h"
#include "DatabaseError.h"
#include "Datagram.h"
#include "GameServerInfoManager.h"
#include "KernelContext.h"
#include "LCSelectPCError.h"
#include "LGIncomingConnection.h"
#include "LoginCharacterSelection.h"
#include "LoginConnection.h"
#include "LoginContext.h"
#include "LoginIncomingRequest.h"
#include "Properties.h"
#include "ServerContext.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "ZoneGroupInfoManager.h"
#include "ZoneInfoManager.h"
#include "repository/ServerInfoRepository.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

class Servers : public ServerInfoRepository {
public:
    bool loadMaxServerGroupID(int& value) override {
        value = 9;
        return true;
    }
    bool loadMaxWorldID(int& value) override {
        value = 7;
        return true;
    }
    std::vector<ServerInfoRow> loadServers() override {
        return {{3, "Selected", "192.0.2.9", 9991, 9992, 7, 9, SERVER_FREE}};
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
};

using Location = std::tuple<int, int, int, std::string>;
using CharacterGroup = std::tuple<int, int, std::string>;
using SelectionRead = std::tuple<int, LoginRaceTable, std::string, std::string>;

class Accounts : public FakeLoginAccountRepository {
public:
    void setCurrentLocation(int world, int group, int slot, const std::string& account) override {
        if (beforeWrite)
            beforeWrite();
        locations.emplace_back(world, group, slot, account);
    }
    std::function<void()> beforeWrite;
    std::vector<Location> locations;
};

class Characters : public FakeLoginCharacterRepository {
public:
    bool loadCharacterForSelect(WorldID_t world, LoginRaceTable table, const std::string& name,
                                const std::string& account, LoginSelectRow& row) override {
        if (beforeRead)
            beforeRead();
        reads.emplace_back(world, table, name, account);
        return FakeLoginCharacterRepository::loadCharacterForSelect(world, table, name, account, row);
    }
    void setCharacterServerGroup(WorldID_t world, int group, const std::string& name) override {
        if (beforeWrite)
            beforeWrite();
        groups.emplace_back(world, group, name);
    }
    std::function<void()> beforeRead;
    std::function<void()> beforeWrite;
    std::vector<SelectionRead> reads;
    std::vector<CharacterGroup> groups;
};

class Topology : public SelectPCTopology {
public:
    bool isNonPKServer(WorldID_t world, ServerGroupID_t group) override {
        ++nonPKReads;
        if (beforeNonPK)
            beforeNonPK(world, group);
        return nonPK;
    }
    ServerID_t zoneServerID(ZoneID_t zone) override {
        ++routeReads;
        if (beforeRoute)
            beforeRoute(zone);
        return 3;
    }
    bool nonPK = false;
    unsigned nonPKReads = 0;
    unsigned routeReads = 0;
    std::function<void(WorldID_t, ServerGroupID_t)> beforeNonPK;
    std::function<void(ZoneID_t)> beforeRoute;
};

struct Session {
    Session() {
        servers.load(rows);
        config.setProperty("User", "elca");
        config.setProperty("GameServerUDPPort", "5678");
        player->setID("account");
        player->setWorldID(7);
        player->setServerGroupID(9);
        player->setPlayerStatus(LPS_PC_MANAGEMENT);
        player->setGameServerIP("192.0.2.1");
        player->loginAccountOwnership().acquire("account", [] { return true; });
        packet.setPCType(PC_SLAYER);
        packet.setPCName("Rowan");
        characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 11, "SLOT2", 30, 1);
    }
    bool select() {
        return de::selectLoginCharacter(*player, packet, topology, incoming, actions, rules);
    }
    Servers rows;
    GameServerInfoManager servers;
    Properties config;
    Accounts accounts;
    Characters characters;
    Topology topology;
    CLSelectPC packet;
    de::LoginConnection player = de::makeLoginConnection(std::make_unique<Socket>("198.51.100.7", 1234));
    unsigned sends = 0;
    std::vector<BYTE> refusals;
    de::LoginIncomingRequestServices incoming{
        servers, config, accounts, characters, [&](const std::string&, uint, const LGIncomingConnection&) { ++sends; },
        {}};
    de::LoginCharacterSelectionActions actions{
        [&](LoginPlayer&, LCSelectPCError& refusal) { refusals.push_back(refusal.getCode()); }, {}};
    de::LoginCharacterSelectionRules rules;
};

TEST(LoginCharacterSelection, AFailedNonPKDiagnosticCannotHideTheRefusalReply) {
    Session session;
    session.topology.nonPK = true;
    session.characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 11, "SLOT2", 81, 3);
    session.actions.diagnostics.nonPKGroup = [](WorldID_t, ServerGroupID_t) {
        throw std::runtime_error("report failed");
    };
    EXPECT_NO_THROW(EXPECT_FALSE(session.select()));
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{SELECT_PC_CANNOT_PLAY_BY_ATTR}));
    EXPECT_EQ(session.sends, 0u);
}

TEST(LoginCharacterSelection, AFailedRoutingDiagnosticCannotSkipTheAcceptedRequest) {
    Session session;
    session.actions.diagnostics.routed = [](WorldID_t, ServerGroupID_t, ServerID_t) {
        throw std::runtime_error("report failed");
    };
    EXPECT_NO_THROW(EXPECT_TRUE(session.select()));
    EXPECT_EQ(session.sends, 1u);
    EXPECT_EQ(session.player->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
}

class RejectedOutput : public std::streambuf {
    int_type overflow(int_type) override {
        return traits_type::eof();
    }
    std::streamsize xsputn(const char*, std::streamsize) override {
        return 0;
    }
};

TEST(LoginCharacterSelection, BrokenProductionOutputCannotSkipDispatch) {
    ASSERT_EXIT(
        {
            Session session;
            session.actions = de::defaultLoginCharacterSelectionActions();
            RejectedOutput output;
            const auto oldExceptions = std::cout.exceptions();
            auto* oldBuffer = std::cout.rdbuf(&output);
            std::cout.exceptions(std::ios::badbit | std::ios::failbit);
            bool selected = false;
            try {
                selected = session.select();
            } catch (...) {
            }
            std::cout.exceptions(std::ios::goodbit);
            std::cout.rdbuf(oldBuffer);
            std::cout.clear();
            std::cout.exceptions(oldExceptions);
            std::_Exit(selected && session.sends == 1 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

void expectPreserved(const Session& session, PlayerStatus status = LPS_PC_MANAGEMENT) {
    EXPECT_EQ(session.player->getPlayerStatus(), status);
    EXPECT_EQ(session.player->getGameServerIP(), "192.0.2.1");
    EXPECT_EQ(session.player->getID(), "account");
    EXPECT_EQ(session.player->getWorldID(), 7);
    EXPECT_EQ(session.player->getServerGroupID(), 9);
    EXPECT_TRUE(session.player->loginAccountOwnership().owns("account"));
    EXPECT_EQ(session.sends, 0u);
    EXPECT_TRUE(session.accounts.locations.empty());
    EXPECT_TRUE(session.characters.groups.empty());
}

TEST(LoginCharacterSelection, RefusalsHaveTheLegacyBytesAndLeaveTheSessionOwnedByTheCaller) {
    for (int gate = 0; gate < 3; ++gate) {
        SCOPED_TRACE(gate);
        Session session;
        if (gate == 0)
            session.rules.agreedToTerms = false;
        else if (gate == 1)
            session.rules.checkFreePlayLimit = true;
        else {
            session.topology.nonPK = true;
            session.characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 11, "SLOT2", 81, 3);
        }
        unsigned replies = 0;
        session.actions.sendRefusal = [&](LoginPlayer& player, LCSelectPCError& packet) {
            EXPECT_EQ(&player, session.player.get());
            expectPreserved(session);
            SocketOutputStream output(nullptr, 64);
            packet.write(output);
            ASSERT_EQ(output.length(), 1u);
            EXPECT_EQ(static_cast<BYTE>(output.getBuffer()[0]), gate == 0 ? 4 : 3);
            ++replies;
        };
        EXPECT_FALSE(session.select());
        EXPECT_EQ(replies, 1u);
        EXPECT_EQ(session.characters.reads.size(), gate == 0 ? 0u : 1u);
        EXPECT_EQ(session.topology.routeReads, 0u);
        expectPreserved(session);
    }
}

TEST(LoginCharacterSelection, EveryOtherPhaseIsFatalBeforeReadsButTermsStillTakePrecedence) {
    for (int value = 0; value < PLAYER_STATUS_MAX; ++value) {
        auto status = static_cast<PlayerStatus>(value);
        if (status == LPS_PC_MANAGEMENT)
            continue;
        SCOPED_TRACE(value);
        Session session;
        session.player->setPlayerStatus(status);
        try {
            (void)session.select();
            FAIL() << "selection did not reject the phase";
        } catch (const DisconnectException& error) {
            EXPECT_EQ(error.getMessage(), "invalid player status");
        }
        EXPECT_TRUE(session.refusals.empty());
        session.rules.agreedToTerms = false;
        EXPECT_FALSE(session.select());
        EXPECT_EQ(session.refusals, (std::vector<BYTE>{4}));
        EXPECT_TRUE(session.characters.reads.empty());
        EXPECT_EQ(session.topology.nonPKReads, 0u);
        EXPECT_EQ(session.topology.routeReads, 0u);
        expectPreserved(session, status);
    }
}

TEST(LoginCharacterSelection, MissingCharactersAndMalformedSlotsKeepTheirFatalMessages) {
    for (bool missing : {true, false}) {
        Session session;
        if (missing)
            session.characters.selectableCharacters.clear();
        else
            session.characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 11, "wrong", 30, 1);
        try {
            (void)session.select();
            FAIL() << "selection did not reject the character";
        } catch (const InvalidProtocolException& error) {
            EXPECT_EQ(error.getMessage(), missing ? "no such PC exist." : "no slot exist.");
        }
        EXPECT_EQ(session.topology.routeReads, 0u);
        EXPECT_TRUE(session.refusals.empty());
        expectPreserved(session);
    }
}

TEST(LoginCharacterSelection, EachRaceDispatchesTheSelectedFieldsBeforeWritingTheOneBasedSlot) {
    const PCType types[] = {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS};
    const LoginRaceTable tables[] = {LOGIN_RACE_TABLE_SLAYER, LOGIN_RACE_TABLE_VAMPIRE, LOGIN_RACE_TABLE_OUSTERS};
    for (int race = 0; race < 3; ++race) {
        Session session;
        session.packet.setPCType(types[race]);
        session.characters.addSelectableCharacter(tables[race], "account", "Rowan", 11, Slot2String[race], 30, 1);
        std::vector<std::string> effects;
        session.topology.beforeNonPK = [&](WorldID_t world, ServerGroupID_t group) {
            EXPECT_EQ(world, 7);
            EXPECT_EQ(group, 9);
            effects.push_back("nonPK");
        };
        session.topology.beforeRoute = [&](ZoneID_t zone) {
            EXPECT_EQ(zone, 11);
            effects.push_back("route");
        };
        session.incoming.send = [&](const std::string& host, uint port, const LGIncomingConnection& packet) {
            EXPECT_EQ(host, "192.0.2.9");
            EXPECT_EQ(port, 5678u);
            EXPECT_EQ(packet.getPlayerID(), "account");
            EXPECT_EQ(packet.getPCName(), "Rowan");
            EXPECT_EQ(packet.getClientIP(), "198.51.100.7");
            EXPECT_EQ(session.player->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
            EXPECT_EQ(session.player->getGameServerIP(), host);
            EXPECT_TRUE(session.accounts.locations.empty());
            EXPECT_TRUE(session.characters.groups.empty());
            effects.push_back("send");
        };
        session.accounts.beforeWrite = [&] { effects.push_back("account"); };
        session.characters.beforeWrite = [&] { effects.push_back("character"); };
        EXPECT_TRUE(session.select());
        EXPECT_EQ(effects, (std::vector<std::string>{"nonPK", "route", "send", "account", "character"}));
        EXPECT_EQ(session.characters.reads, (std::vector<SelectionRead>{{7, tables[race], "Rowan", "account"}}));
        EXPECT_EQ(session.accounts.locations, (std::vector<Location>{{7, 9, race + 1, "account"}}));
        EXPECT_EQ(session.characters.groups, (std::vector<CharacterGroup>{{7, 9, "Rowan"}}));
        EXPECT_TRUE(session.player->loginAccountOwnership().owns("account"));
        EXPECT_TRUE(session.refusals.empty());
    }
}

TEST(LoginCharacterSelection, ExplicitFreePlayRulesUseTheRaceLimitAndDefaultToDisabled) {
    const PCType types[] = {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS};
    const LoginRaceTable tables[] = {LOGIN_RACE_TABLE_SLAYER, LOGIN_RACE_TABLE_VAMPIRE, LOGIN_RACE_TABLE_OUSTERS};
    for (int race = 0; race < 3; ++race) {
        for (bool limitSlayer : {false, true}) {
            Session session;
            session.packet.setPCType(types[race]);
            session.characters.addSelectableCharacter(tables[race], "account", "Rowan", 11, "SLOT2", 30, 1);
            session.rules.checkFreePlayLimit = true;
            session.rules.freePlaySlayerDomainSum = limitSlayer ? 29 : 30;
            session.rules.freePlayVampireLevel = limitSlayer ? 30 : 29;
            const bool accepted = (race == 0) != limitSlayer;
            EXPECT_EQ(session.select(), accepted);
            if (!accepted) {
                expectPreserved(session);
                EXPECT_EQ(session.refusals, (std::vector<BYTE>{3}));
                session.rules.checkFreePlayLimit = false;
                EXPECT_TRUE(session.select());
            }
        }
    }
}

TEST(LoginCharacterSelection, ThePacketAndRulesAreOwnedBeforeTheFirstRepositoryCallback) {
    Session session;
    session.characters.beforeRead = [&] {
        session.packet.setPCName("replacement");
        session.packet.setPCType(PC_VAMPIRE);
        session.rules.agreedToTerms = false;
        session.rules.checkFreePlayLimit = true;
    };
    session.incoming.send = [&](const std::string&, uint, const LGIncomingConnection& packet) {
        EXPECT_EQ(packet.getPCName(), "Rowan");
        EXPECT_EQ(packet.getPlayerID(), "account");
    };
    EXPECT_TRUE(session.select());
    EXPECT_EQ(session.characters.reads, (std::vector<SelectionRead>{{7, LOGIN_RACE_TABLE_SLAYER, "Rowan", "account"}}));
    EXPECT_EQ(session.characters.groups, (std::vector<CharacterGroup>{{7, 9, "Rowan"}}));
}

enum class Stage { Read, NonPK, Route, Refusal, Send, Account, Character };
constexpr Stage stages[] = {Stage::Read, Stage::NonPK,   Stage::Route,    Stage::Refusal,
                            Stage::Send, Stage::Account, Stage::Character};

void failAt(Session& session, Stage stage, std::exception_ptr failure) {
    auto fail = [failure] { std::rethrow_exception(failure); };
    switch (stage) {
    case Stage::Read:
        session.characters.beforeRead = fail;
        break;
    case Stage::NonPK:
        session.topology.beforeNonPK = [fail](WorldID_t, ServerGroupID_t) { fail(); };
        break;
    case Stage::Route:
        session.topology.beforeRoute = [fail](ZoneID_t) { fail(); };
        break;
    case Stage::Refusal:
        session.rules.agreedToTerms = false;
        session.actions.sendRefusal = [fail](LoginPlayer&, LCSelectPCError&) { fail(); };
        break;
    case Stage::Send:
        session.incoming.send = [fail](const std::string&, uint, const LGIncomingConnection&) { fail(); };
        break;
    case Stage::Account:
        session.accounts.beforeWrite = fail;
        break;
    case Stage::Character:
        session.characters.beforeWrite = fail;
        break;
    }
}

void expectAfterFailure(const Session& session, Stage stage) {
    if (stage == Stage::Account || stage == Stage::Character) {
        EXPECT_EQ(session.sends, 1u);
        EXPECT_EQ(session.player->getPlayerStatus(), LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
        EXPECT_EQ(session.player->getGameServerIP(), "192.0.2.9");
        EXPECT_EQ(session.accounts.locations.size(), stage == Stage::Character ? 1u : 0u);
        EXPECT_TRUE(session.characters.groups.empty());
        EXPECT_TRUE(session.player->loginAccountOwnership().owns("account"));
    } else {
        expectPreserved(session);
    }
    EXPECT_TRUE(session.refusals.empty());
}

TEST(LoginCharacterSelection, DatabaseFailuresKeepTheDisconnectPrefixAndAnyCompletedEffects) {
    for (const auto stage : stages) {
        SCOPED_TRACE(static_cast<int>(stage));
        Session session;
        failAt(session, stage, std::make_exception_ptr(DatabaseError("selection database failed")));
        try {
            (void)session.select();
            FAIL() << "database failure was swallowed";
        } catch (const DisconnectException& error) {
            EXPECT_EQ(error.getMessage(), "CLSelectPCHandler : selection database failed");
        }
        expectAfterFailure(session, stage);
    }
}

TEST(LoginCharacterSelection, MissingDataKeepsTheIntegrityErrorAndAnyCompletedEffects) {
    const NoSuchElementException missing("selection reference missing");
    for (const auto stage : stages) {
        SCOPED_TRACE(static_cast<int>(stage));
        Session session;
        failAt(session, stage, std::make_exception_ptr(missing));
        try {
            (void)session.select();
            FAIL() << "missing data was swallowed";
        } catch (const Error& error) {
            EXPECT_EQ(error.getMessage(),
                      "Critical Error : data intergrity broken at ZoneInfo - ZoneGroupInfo - GameServerInfo : " +
                          missing.toString());
        }
        expectAfterFailure(session, stage);
    }
}

TEST(LoginCharacterSelection, AnAbsentDestinationIsTranslatedBeforePublishingTheWait) {
    Session session;
    session.servers.clear();
    EXPECT_THROW((void)session.select(), Error);
    expectPreserved(session);
    session.servers.load(session.rows);
    EXPECT_TRUE(session.select());
}

TEST(LoginCharacterSelection, OtherExceptionsRetainTheirIdentityAtEveryStage) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(Error("configuration failure")),
                                           std::make_exception_ptr(ConnectException("send failure")),
                                           std::make_exception_ptr(std::runtime_error("callback failure")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (const auto stage : stages) {
            SCOPED_TRACE(static_cast<int>(stage));
            Session session;
            failAt(session, stage, failure);
            try {
                (void)session.select();
                FAIL() << "failure was swallowed";
            } catch (...) {
                EXPECT_EQ(std::current_exception(), failure);
            }
            expectAfterFailure(session, stage);
        }
    }
}

TEST(LoginCharacterSelection, DiagnosticExceptionsCannotReplaceALaterOperationFailure) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("report database failure")),
                                           std::make_exception_ptr(NoSuchElementException("report reference missing")),
                                           std::make_exception_ptr(Error("report failure")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        Session session;
        session.topology.nonPK = true;
        unsigned reports = 0;
        session.actions.diagnostics.nonPKGroup = [&](WorldID_t world, ServerGroupID_t group) {
            EXPECT_EQ(world, 7);
            EXPECT_EQ(group, 9);
            ++reports;
            std::rethrow_exception(failure);
        };
        session.actions.diagnostics.routed = [&](WorldID_t world, ServerGroupID_t group, ServerID_t server) {
            EXPECT_EQ(world, 7);
            EXPECT_EQ(group, 9);
            EXPECT_EQ(server, 3);
            ++reports;
            std::rethrow_exception(failure);
        };
        failAt(session, Stage::Send, std::make_exception_ptr(DatabaseError("actual send failure")));
        try {
            (void)session.select();
            FAIL() << "send failure was swallowed";
        } catch (const DisconnectException& error) {
            EXPECT_EQ(error.getMessage(), "CLSelectPCHandler : actual send failure");
        }
        EXPECT_EQ(reports, 2u);
        expectPreserved(session);
    }
}

TEST(LoginCharacterSelection, TheProductionHandlerRejectsBeforeRequiringASenderOrDatabase) {
    ASSERT_EXIT(
        {
            Session session;
            ZoneInfoManager zones;
            ZoneGroupInfoManager groups;
            de::kernelContext().setConfig(&session.config);
            de::serverContext().setGameServerInfoManager(&session.servers);
            de::loginContext().setZoneInfoManager(&zones);
            de::loginContext().setZoneGroupInfoManager(&groups);
            de::loginContext().setGameServerManager(nullptr);
            session.player->setPlayerStatus(LPS_BEGIN_SESSION);
            try {
                CLSelectPCHandler::execute(&session.packet, session.player.get());
            } catch (const DisconnectException& error) {
                std::_Exit(error.getMessage() == "invalid player status" &&
                                   session.player->getPlayerStatus() == LPS_BEGIN_SESSION &&
                                   session.player->loginAccountOwnership().owns("account")
                               ? 0
                               : 1);
            } catch (...) {
                std::_Exit(2);
            }
            std::_Exit(3);
        },
        ::testing::ExitedWithCode(0), "");
}

int checkAllocationFailure(std::size_t position, int mode) {
    auto session = std::make_unique<Session>();
    const std::string account(mode == 0 ? 20 : 64, 'a');
    const std::string name(mode == 0 ? 20 : 64, 'n');
    const std::string previous(96, 'p');
    session->player->setID(account);
    session->player->setGameServerIP(previous);
    const auto* borrowed = session->player->getGameServerIP().data();
    session->packet.setPCName(name);
    session->characters.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, account, name, 11, "SLOT2", 30, 1);
    session->config.setProperty("User", std::string(96, 'u'));
    session->config.setProperty("GameServerUDPPort", std::string(96, ' ') + "5678");
    if (mode == 1)
        session->rules.agreedToTerms = false;
    if (mode == 2)
        failAt(*session, Stage::Read, std::make_exception_ptr(DatabaseError(std::string(96, 'd'))));
    if (mode == 3)
        failAt(*session, Stage::Route, std::make_exception_ptr(NoSuchElementException(std::string(96, 'm'))));
    session->incoming.send = [&](const std::string&, uint, const LGIncomingConnection& packet) {
        Datagram frame;
        frame.write(&packet);
        ++session->sends;
    };
    AllocationProbe probe(position);
    bool allocationFailed = false;
    bool finished = false;
    try {
        const bool selected = session->select();
        finished = (mode == 0 && selected) || (mode == 1 && !selected);
    } catch (const std::bad_alloc&) {
        allocationFailed = true;
    } catch (const DisconnectException&) {
        finished = mode == 2;
    } catch (const Error&) {
        finished = mode == 3;
    } catch (...) {
        return 1;
    }
    probe.stopFailing();
    if (allocationFailed != probe.rejected() || (!allocationFailed && !finished) ||
        (position == 64 && allocationFailed))
        return 2;
    if (!session->player->loginAccountOwnership().owns("account"))
        return 3;
    if (session->sends == 0) {
        if (session->player->getPlayerStatus() != LPS_PC_MANAGEMENT || session->player->getGameServerIP() != previous ||
            session->player->getGameServerIP().data() != borrowed || !session->accounts.locations.empty() ||
            !session->characters.groups.empty())
            return 4;
        if (allocationFailed && mode < 2 && session->select() != (mode == 0))
            return 5;
    } else if (mode != 0 || session->player->getPlayerStatus() != LPS_AFTER_SENDING_LG_INCOMING_CONNECTION ||
               session->player->getGameServerIP() != "192.0.2.9") {
        return 6;
    }
    session.reset();
    return probe.outstanding() == 0 ? 0 : 7;
}

TEST(LoginCharacterSelection, AllocationFailurePreservesOwnershipAndPartialEffectsWithoutLeaking) {
    for (int mode = 0; mode < 4; ++mode) {
        for (std::size_t position = 1; position <= 64; ++position) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << position);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(position, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
