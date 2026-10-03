#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "CLRegisterPlayer.h"
#include "DatabaseError.h"
#include "GameServerGroupInfoManager.h"
#include "KernelContext.h"
#include "LCRegisterPlayerError.h"
#include "LCRegisterPlayerOK.h"
#include "LoginAccountSession.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "LoginRegistration.h"
#include "PasswordHash.h"
#include "Properties.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "repository/LoginConfigRepository.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"

namespace {

enum class Stage { Hash, Exists, Connection, Insert, Acquire, Location, Group, Refusal, Success, Report };

class Accounts : public FakeLoginAccountRepository {
public:
    void visit(Stage stage) {
        calls.push_back(stage);
        if (before)
            before(stage);
        if (stage == failureStage && failure)
            std::rethrow_exception(failure);
    }
    bool accountExists(const std::string& account) override {
        visit(Stage::Exists);
        queries.push_back(account);
        return FakeLoginAccountRepository::accountExists(account);
    }
    void insertAccount(const LoginNewAccount& account) override {
        visit(Stage::Insert);
        auto rows = insertedAccounts;
        auto ids = registeredIDs;
        rows.push_back(account);
        ids.insert(account.playerID);
        insertedAccounts.swap(rows);
        registeredIDs.swap(ids);
        if (after)
            after(Stage::Insert);
    }
    void markLoggedOnAfterRegister(const std::string& ip, int server, const std::string& account) override {
        visit(Stage::Acquire);
        FakeLoginAccountRepository::markLoggedOnAfterRegister(ip, server, account);
        if (after)
            after(Stage::Acquire);
    }
    bool loadCurrentLocation(LoginLocationSpelling spelling, const std::string& account, int& world,
                             int& group) override {
        visit(Stage::Location);
        locationSpelling = spelling;
        locationAccount = account;
        world = currentWorld;
        group = currentGroup;
        return locationFound;
    }
    void markLoggedOff(const std::string& account) override {
        logouts.push_back(account);
    }
    std::vector<Stage> calls;
    std::vector<std::string> queries;
    std::vector<std::string> logouts;
    std::function<void(Stage)> before;
    std::function<void(Stage)> after;
    Stage failureStage = Stage::Exists;
    std::exception_ptr failure;
    int currentWorld = 7;
    int currentGroup = 9;
    bool locationFound = true;
    LoginLocationSpelling locationSpelling = LOGIN_LOCATION_SQL_LOWER;
    std::string locationAccount;
};

class RegistrationPlayer : public LoginPlayer {
public:
    RegistrationPlayer() : LoginPlayer(new Socket("198.51.100.7", 1234)) {
        setID("guest");
        setWorldID(3);
        setServerGroupID(2);
        setFailureCount(0);
        setPlayerStatus(LPS_BEGIN_SESSION);
        setAdult(false);
    }
    ~RegistrationPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
    void sendPacket(Packet* packet) override {
        if (observe)
            observe(*packet);
        LoginPlayer::sendPacket(packet);
        ++sent;
    }
    std::string bufferedBytes() const {
        return {m_pOutputStream->getBuffer(), m_pOutputStream->length()};
    }
    unsigned sent = 0;
    std::function<void(Packet&)> observe;
};

struct Session {
    Session() {
        packet.setID("newcomer");
        packet.setPassword("test-pass");
        packet.setName("Example Player");
        packet.setSex(FEMALE);
        packet.setSSN("test-ssn");
        packet.setTelephone("test-phone");
        packet.setCellular("test-mobile");
        packet.setZipCode("123-456");
        packet.setAddress("test-address");
        packet.setNation(static_cast<Nation>(1));
        packet.setEmail("test@example.com");
        packet.setHomepage("example.com");
        packet.setProfile("test-profile");
        packet.setPublic(true);
    }
    bool registerPlayer() {
        return de::registerLoginPlayer(player, packet, accounts, actions);
    }
    Accounts accounts;
    RegistrationPlayer player;
    CLRegisterPlayer packet;
    de::LoginRegistrationConnection connection{"198.51.100.7", 6};
    std::string groupName = "Seven";
    std::vector<std::pair<int, int>> groups;
    std::vector<BYTE> refusals;
    unsigned successes = 0;
    unsigned reports = 0;
    de::LoginRegistrationActions actions{{[&](const std::string&) {
                                             accounts.visit(Stage::Hash);
                                             return std::string("encoded-test-hash");
                                         }},
                                         [&](const LoginPlayer&) {
                                             accounts.visit(Stage::Connection);
                                             return connection;
                                         },
                                         [&](WorldID_t world, ServerGroupID_t group) {
                                             accounts.visit(Stage::Group);
                                             groups.emplace_back(world, group);
                                             return groupName;
                                         },
                                         [&](LoginPlayer&, LCRegisterPlayerError& reply) {
                                             accounts.visit(Stage::Refusal);
                                             refusals.push_back(reply.getErrorID());
                                         },
                                         [&](LoginPlayer&, LCRegisterPlayerOK&) {
                                             accounts.visit(Stage::Success);
                                             ++successes;
                                         },
                                         [&](const std::string&, const std::string&) {
                                             accounts.visit(Stage::Report);
                                             ++reports;
                                         }};
};

void expectOriginalSelection(const Session& session) {
    EXPECT_EQ(session.player.getID(), "guest");
    EXPECT_EQ(session.player.getWorldID(), 3);
    EXPECT_EQ(session.player.getServerGroupID(), 2);
    EXPECT_FALSE(session.player.isAdult());
}

TEST(LoginRegistration, SuccessPublishesTheReadBackWorldGroupAndIdentityAfterSending) {
    Session session;
    session.actions.sendSuccess = [&](LoginPlayer&, LCRegisterPlayerOK& reply) {
        expectOriginalSelection(session);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
        EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
        EXPECT_EQ(reply.getGroupName(), "Seven");
        EXPECT_TRUE(reply.isAdult());
        ++session.successes;
    };
    EXPECT_TRUE(session.registerPlayer());
    EXPECT_EQ(session.player.getID(), "newcomer");
    EXPECT_EQ(session.player.getWorldID(), 7);
    EXPECT_EQ(session.player.getServerGroupID(), 9);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
}

TEST(LoginRegistration, InvalidLocationIDsFailBeforeGroupLookupOrSessionPublication) {
    for (const auto [world, group] : {std::pair{-1, 9}, {256, 9}, {7, -1}, {7, 256}}) {
        Session session;
        session.accounts.currentWorld = world;
        session.accounts.currentGroup = group;
        EXPECT_THROW(session.registerPlayer(), Error);
        EXPECT_TRUE(session.groups.empty());
        EXPECT_EQ(session.successes, 0u);
        expectOriginalSelection(session);
        EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
    }
}

TEST(LoginRegistration, GroupAndSendFailuresKeepTheOriginalSelectionAndAcquiredOwner) {
    for (const auto stage : {Stage::Group, Stage::Success}) {
        Session session;
        session.accounts.failureStage = stage;
        session.accounts.failure = std::make_exception_ptr(std::runtime_error("operation failed"));
        EXPECT_THROW(session.registerPlayer(), std::runtime_error);
        expectOriginalSelection(session);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
        EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
    }
}

TEST(LoginRegistration, DiagnosticFailureCannotReplaceTheHashingDisconnectOrSendAnotherRefusal) {
    Session session;
    session.actions.decision.hashPassword = [](const std::string&) -> std::string {
        throw std::runtime_error("synthetic hashing failure");
    };
    session.accounts.failureStage = Stage::Report;
    session.accounts.failure = std::make_exception_ptr(DatabaseError("diagnostic failed"));
    try {
        (void)session.registerPlayer();
        FAIL() << "hashing failure did not disconnect";
    } catch (const DisconnectException& error) {
        EXPECT_EQ(error.getMessage(), "password hashing failed");
    }
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{ETC_ERROR}));
    EXPECT_EQ(session.player.getFailureCount(), 0u);
}

TEST(LoginRegistration, TheRetryLimitDoesNotWrapAnAlreadyExhaustedCounter) {
    Session session;
    session.accounts.registeredIDs.insert("newcomer");
    session.player.setFailureCount(std::numeric_limits<uint>::max());
    EXPECT_THROW(session.registerPlayer(), DisconnectException);
    EXPECT_EQ(session.player.getFailureCount(), std::numeric_limits<uint>::max());
}

TEST(LoginRegistration, AcceptedRegistrationPreservesQueryWriteAndReplyOrder) {
    Session session;
    EXPECT_TRUE(session.registerPlayer());
    EXPECT_EQ(session.accounts.calls,
              (std::vector<Stage>{Stage::Hash, Stage::Exists, Stage::Connection, Stage::Insert, Stage::Acquire,
                                  Stage::Location, Stage::Group, Stage::Success}));
    EXPECT_EQ(session.accounts.queries, (std::vector<std::string>{"newcomer"}));
    EXPECT_EQ(session.accounts.locationSpelling, LOGIN_LOCATION_SQL_UPPER);
    EXPECT_EQ(session.accounts.locationAccount, "newcomer");
    EXPECT_EQ(session.groups, (std::vector<std::pair<int, int>>{{7, 9}}));
    ASSERT_EQ(session.accounts.insertedAccounts.size(), 1u);
    const auto& row = session.accounts.insertedAccounts.front();
    EXPECT_EQ(row.playerID, "newcomer");
    EXPECT_TRUE(row.password == "encoded-test-hash");
    EXPECT_EQ(row.name, "Example Player");
    EXPECT_EQ(row.sex, "FEMALE");
    EXPECT_EQ(row.ssn, "test-ssn");
    EXPECT_EQ(row.telephone, "test-phone");
    EXPECT_EQ(row.cellular, "test-mobile");
    EXPECT_EQ(row.zipCode, "123-456");
    EXPECT_EQ(row.address, "test-address");
    EXPECT_EQ(row.nation, 1);
    EXPECT_EQ(row.email, "test@example.com");
    EXPECT_EQ(row.homepage, "example.com");
    EXPECT_EQ(row.profile, "test-profile");
    EXPECT_EQ(row.pub, "PUBLIC");
    ASSERT_EQ(session.accounts.markedLoggedOnAfterRegister.size(), 1u);
    const auto& acquisition = session.accounts.markedLoggedOnAfterRegister.front();
    EXPECT_EQ(acquisition.ip, "198.51.100.7");
    EXPECT_EQ(acquisition.loginServerID, 6);
    EXPECT_EQ(acquisition.playerID, "newcomer");
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
    EXPECT_EQ(session.player.getFailureCount(), 0u);
    EXPECT_FALSE(session.player.isAdult());
}

struct InvalidRequest {
    std::function<void(CLRegisterPlayer&)> prepare;
    BYTE code;
};

const InvalidRequest invalidRequests[] = {{[](CLRegisterPlayer& packet) { packet.setID(""); }, 4},
                                          {[](CLRegisterPlayer& packet) { packet.setID("abc"); }, 5},
                                          {[](CLRegisterPlayer& packet) { packet.setID("bad'id"); }, 15},
                                          {[](CLRegisterPlayer& packet) { packet.setPassword(""); }, 6},
                                          {[](CLRegisterPlayer& packet) { packet.setPassword("short"); }, 7},
                                          {[](CLRegisterPlayer& packet) { packet.setName(""); }, 8},
                                          {[](CLRegisterPlayer& packet) { packet.setSSN(""); }, 9},
                                          {[](CLRegisterPlayer& packet) { packet.setProfile("invalid'"); }, 15}};

TEST(LoginRegistration, ValidationRefusalsKeepTheirLiteralBytesAndDisconnectBeforeHashingOrConfiguration) {
    for (const auto& invalid : invalidRequests) {
        Session session;
        invalid.prepare(session.packet);
        session.actions.sendRefusal = de::defaultLoginRegistrationActions().sendRefusal;
        try {
            (void)session.registerPlayer();
            FAIL() << "validation refusal did not disconnect";
        } catch (const DisconnectException& error) {
            EXPECT_EQ(error.getMessage(), "LCRegisterPlayerError(" + std::to_string(invalid.code) + ")");
        }
        EXPECT_TRUE(session.accounts.calls.empty());
        EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), std::string(1, invalid.code));
        EXPECT_EQ(session.player.getFailureCount(), 0u);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
        EXPECT_EQ(session.player.loginAccountOwnership().account(), nullptr);
        expectOriginalSelection(session);
    }
}

TEST(LoginRegistration, DuplicateIDsAllowThreeFailuresAndSendTheFourthRefusalBeforeDisconnecting) {
    Session session;
    session.accounts.registeredIDs.insert("newcomer");
    for (uint attempt = 1; attempt <= 3; ++attempt) {
        EXPECT_FALSE(session.registerPlayer());
        EXPECT_EQ(session.player.getFailureCount(), attempt);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
    }
    try {
        (void)session.registerPlayer();
        FAIL() << "fourth refusal did not disconnect";
    } catch (const DisconnectException& error) {
        EXPECT_EQ(error.getMessage(), "too many failure");
    }
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{2, 2, 2, 2}));
    EXPECT_EQ(session.player.getFailureCount(), 3u);
    EXPECT_EQ(session.accounts.accountExistsCalls, 4);
    EXPECT_TRUE(session.accounts.insertedAccounts.empty());
    EXPECT_EQ(session.player.loginAccountOwnership().account(), nullptr);
    expectOriginalSelection(session);
}

TEST(LoginRegistration, MissingReadBackRetainsAcquisitionAndCannotRestartRegistrationForAnotherAccount) {
    Session session;
    session.accounts.locationFound = false;
    EXPECT_FALSE(session.registerPlayer());
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{15}));
    EXPECT_EQ(session.player.getFailureCount(), 1u);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
    EXPECT_TRUE(session.groups.empty());
    expectOriginalSelection(session);
    EXPECT_FALSE(session.registerPlayer());
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{15, 2}));
    session.packet.setID("different");
    EXPECT_THROW(session.registerPlayer(), DisconnectException);
    EXPECT_EQ(session.accounts.insertedAccounts.size(), 1u);
    EXPECT_EQ(session.accounts.markedLoggedOnAfterRegister.size(), 1u);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
    de::disconnectLoginPlayer(session.player, session.accounts, false, {[] {}, [] {}});
    EXPECT_EQ(session.accounts.logouts, (std::vector<std::string>{"newcomer"}));
}

constexpr Stage operationStages[] = {Stage::Exists,   Stage::Connection, Stage::Insert, Stage::Acquire,
                                     Stage::Location, Stage::Group,      Stage::Success};

TEST(LoginRegistration, DatabaseFailuresSendOneGenericRefusalAndRetainAcknowledgedAcquisition) {
    for (const auto stage : operationStages) {
        Session session;
        session.accounts.failureStage = stage;
        session.accounts.failure = std::make_exception_ptr(DatabaseError("operation failed"));
        EXPECT_FALSE(session.registerPlayer());
        EXPECT_EQ(session.refusals, (std::vector<BYTE>{15}));
        EXPECT_EQ(session.player.getFailureCount(), 1u);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
        EXPECT_EQ(session.player.loginAccountOwnership().owns("newcomer"), stage >= Stage::Location);
        EXPECT_EQ(session.successes, 0u);
        expectOriginalSelection(session);
    }
}

TEST(LoginRegistration, OtherOperationFailuresKeepTheirIdentityAndPartialPersistence) {
    const std::exception_ptr failures[] = {
        std::make_exception_ptr(NoSuchElementException("missing result")),
        std::make_exception_ptr(Error("operation failed")), std::make_exception_ptr(DisconnectException("send failed")),
        std::make_exception_ptr(std::runtime_error("standard failure")), std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (const auto stage : operationStages) {
            Session session;
            session.accounts.failureStage = stage;
            session.accounts.failure = failure;
            try {
                (void)session.registerPlayer();
                FAIL() << "failure was swallowed";
            } catch (...) {
                EXPECT_EQ(std::current_exception(), failure);
            }
            EXPECT_TRUE(session.refusals.empty());
            EXPECT_EQ(session.successes, 0u);
            EXPECT_EQ(session.player.getFailureCount(), 0u);
            EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
            EXPECT_EQ(session.accounts.insertedAccounts.size(), stage >= Stage::Acquire ? 1u : 0u);
            EXPECT_EQ(session.player.loginAccountOwnership().owns("newcomer"), stage >= Stage::Location);
            expectOriginalSelection(session);
        }
    }
}

TEST(LoginRegistration, InitialRefusalDatabaseFailureGetsOneGenericFallbackAndOnlyThenCounts) {
    Session session;
    session.packet.setID("");
    std::vector<BYTE> attempts;
    session.actions.sendRefusal = [&](LoginPlayer&, LCRegisterPlayerError& reply) {
        attempts.push_back(reply.getErrorID());
        if (attempts.size() == 1)
            throw DatabaseError("initial refusal failed");
    };
    EXPECT_FALSE(session.registerPlayer());
    EXPECT_EQ(attempts, (std::vector<BYTE>{4, 15}));
    EXPECT_EQ(session.player.getFailureCount(), 1u);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_REGISTER_PLAYER);
    expectOriginalSelection(session);
}

TEST(LoginRegistration, FailureSendingTheGenericFallbackPropagatesWithoutAnotherReplyOrCount) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(DatabaseError("fallback failed")),
                                           std::make_exception_ptr(DisconnectException("fallback failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        Session session;
        session.accounts.failureStage = Stage::Location;
        session.accounts.failure = std::make_exception_ptr(DatabaseError("read-back failed"));
        unsigned attempts = 0;
        session.actions.sendRefusal = [&](LoginPlayer&, LCRegisterPlayerError& reply) {
            EXPECT_EQ(reply.getErrorID(), 15);
            ++attempts;
            std::rethrow_exception(failure);
        };
        try {
            (void)session.registerPlayer();
            FAIL() << "fallback failure was swallowed";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        EXPECT_EQ(attempts, 1u);
        EXPECT_EQ(session.player.getFailureCount(), 0u);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
        EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
        expectOriginalSelection(session);
    }
}

TEST(LoginRegistration, WritesThatThrowAfterApplyingTheirEffectRemainUnacknowledgedAndCannotBeReplayed) {
    for (const auto stage : {Stage::Insert, Stage::Acquire}) {
        Session session;
        session.accounts.after = [&](Stage current) {
            if (stage == current)
                throw DatabaseError("write acknowledgement failed");
        };
        EXPECT_FALSE(session.registerPlayer());
        EXPECT_EQ(session.accounts.insertedAccounts.size(), 1u);
        EXPECT_EQ(session.accounts.markedLoggedOnAfterRegister.size(), stage == Stage::Acquire ? 1u : 0u);
        EXPECT_EQ(session.player.loginAccountOwnership().account(), nullptr);
        session.accounts.after = {};
        EXPECT_FALSE(session.registerPlayer());
        EXPECT_EQ(session.refusals, (std::vector<BYTE>{15, 2}));
        EXPECT_EQ(session.accounts.insertedAccounts.size(), 1u);
        expectOriginalSelection(session);
    }
}

TEST(LoginRegistration, PacketAndConnectionMutationCannotChangeThePreparedInputs) {
    Session session;
    session.accounts.before = [&](Stage stage) {
        if (stage == Stage::Hash) {
            session.packet.setID("different");
            session.packet.setName("Changed");
            session.packet.setSex(MALE);
            session.packet.setProfile("changed-profile");
        }
        if (stage == Stage::Insert) {
            session.connection.ip = "203.0.113.9";
            session.connection.loginServerID = 99;
        }
    };
    EXPECT_TRUE(session.registerPlayer());
    ASSERT_EQ(session.accounts.insertedAccounts.size(), 1u);
    EXPECT_EQ(session.accounts.insertedAccounts.front().playerID, "newcomer");
    EXPECT_EQ(session.accounts.insertedAccounts.front().name, "Example Player");
    EXPECT_EQ(session.accounts.insertedAccounts.front().sex, "FEMALE");
    EXPECT_EQ(session.accounts.insertedAccounts.front().profile, "test-profile");
    EXPECT_EQ(session.accounts.markedLoggedOnAfterRegister.front().ip, "198.51.100.7");
    EXPECT_EQ(session.accounts.markedLoggedOnAfterRegister.front().loginServerID, 6);
    EXPECT_EQ(session.player.getID(), "newcomer");
    EXPECT_EQ(session.accounts.locationAccount, "newcomer");
}

TEST(LoginRegistration, ZeroAndMaximumCatalogueLocationsArePublishedWithoutNarrowing) {
    for (const auto [world, group] : {std::pair{0, 255}, {255, 0}, {255, 255}}) {
        Session session;
        session.accounts.currentWorld = world;
        session.accounts.currentGroup = group;
        EXPECT_TRUE(session.registerPlayer());
        EXPECT_EQ(session.groups, (std::vector<std::pair<int, int>>{{world, group}}));
        EXPECT_EQ(session.player.getWorldID(), world);
        EXPECT_EQ(session.player.getServerGroupID(), group);
    }
}

TEST(LoginRegistration, EveryHashingDiagnosticFailurePreservesTheSingleRefusalAndDisconnect) {
    const std::exception_ptr failures[] = {
        std::make_exception_ptr(DatabaseError("report failed")), std::make_exception_ptr(Error("report failed")),
        std::make_exception_ptr(std::runtime_error("report failed")), std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        Session session;
        session.actions.decision.hashPassword = [](const std::string&) -> std::string {
            throw std::runtime_error("synthetic hashing failure");
        };
        session.actions.hashingFailure = [&](const std::string& account, const std::string& detail) {
            EXPECT_EQ(account, "newcomer");
            EXPECT_EQ(detail, "synthetic hashing failure");
            std::rethrow_exception(failure);
        };
        try {
            (void)session.registerPlayer();
            FAIL() << "hashing failure did not disconnect";
        } catch (const DisconnectException& error) {
            EXPECT_EQ(error.getMessage(), "password hashing failed");
        }
        EXPECT_EQ(session.refusals, (std::vector<BYTE>{15}));
        EXPECT_EQ(session.player.getFailureCount(), 0u);
        EXPECT_TRUE(session.accounts.queries.empty());
        expectOriginalSelection(session);
    }
}

TEST(LoginRegistration, TheProductionSenderPreservesSuccessBytesAndGroupNameTruncation) {
    for (bool longName : {false, true}) {
        Session session;
        session.groupName = longName ? std::string(maxNameLength + 5, 'g') : "Seven";
        session.actions.sendSuccess = de::defaultLoginRegistrationActions().sendSuccess;
        session.player.observe = [&](Packet& reply) {
            EXPECT_EQ(reply.getPacketID(), Packet::PACKET_LC_REGISTER_PLAYER_OK);
            expectOriginalSelection(session);
            EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
        };
        EXPECT_TRUE(session.registerPlayer());
        const auto name = session.groupName.substr(0, maxNameLength);
        std::string expected(1, static_cast<char>(name.size()));
        expected += name;
        expected.push_back('\1');
        EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), expected);
        EXPECT_EQ(session.player.sent, 1u);
    }
}

TEST(LoginRegistration, AnEmptyGroupNameFailsProductionSerializationAndRetainsTheAcquiredOwner) {
    Session session;
    session.groupName.clear();
    session.actions.sendSuccess = de::defaultLoginRegistrationActions().sendSuccess;
    EXPECT_THROW(session.registerPlayer(), InvalidProtocolException);
    EXPECT_TRUE(session.player.bufferedBytes().empty());
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    expectOriginalSelection(session);
}

TEST(LoginRegistration, FailureAfterBufferingRetainsTheReplyAndOwnerWithoutPublishingIdentity) {
    Session session;
    session.actions.sendSuccess = [&](LoginPlayer& player, LCRegisterPlayerOK& reply) {
        player.sendPacket(&reply);
        throw DisconnectException("failure after buffering");
    };
    EXPECT_THROW(session.registerPlayer(), DisconnectException);
    EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), std::string("\5Seven\1", 7));
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
    expectOriginalSelection(session);
    EXPECT_FALSE(session.registerPlayer());
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{2}));
    EXPECT_EQ(session.accounts.insertedAccounts.size(), 1u);
    EXPECT_EQ(session.player.getFailureCount(), 1u);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("newcomer"));
}

TEST(LoginRegistration, TheProductionHandlerCanRefuseWithoutDatabaseOrProcessConfiguration) {
    Session session;
    session.packet.setID("bad'id");
    EXPECT_THROW(CLRegisterPlayerHandler::execute(&session.packet, &session.player), DisconnectException);
    EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), std::string(1, '\17'));
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_EQ(session.player.loginAccountOwnership().account(), nullptr);
}

int checkPublication() {
    Session session;
    std::optional<AllocationProbe> probe;
    session.actions.sendSuccess = [&](LoginPlayer&, LCRegisterPlayerOK&) { probe.emplace(1); };
    if (!session.registerPlayer())
        return 1;
    probe->stopFailing();
    return probe->attempts() == 0 && session.player.getID() == "newcomer" && session.player.getWorldID() == 7 &&
                   session.player.getServerGroupID() == 9 &&
                   session.player.getPlayerStatus() == LPS_WAITING_FOR_CL_GET_PC_LIST
               ? 0
               : 2;
}

TEST(LoginRegistration, PublicationAfterSendingDoesNotAllocate) {
    ASSERT_EXIT(std::_Exit(checkPublication()), ::testing::ExitedWithCode(0), "");
}

class GroupRows : public LoginConfigRepository {
public:
    bool loadMaxGameServerGroupWorldID(int& maximum) override {
        maximum = 7;
        return true;
    }
    std::vector<LoginGameServerGroupRow> loadGameServerGroups() override {
        return {{7, 9, name, 0}};
    }
    std::vector<LoginGameServerGroupIDRow> loadGameServerGroupIDs() override {
        return {};
    }
    std::vector<LoginZoneGroupRow> loadZoneGroups() override {
        return {};
    }
    std::vector<LoginZoneRow> loadZones() override {
        return {};
    }
    bool loadClientVersion(int&) override {
        return false;
    }
    std::string name = "Seven";
};

int checkProductionAdapters() {
    GroupRows rows;
    GameServerGroupInfoManager groups;
    groups.load(rows);
    Properties config;
    config.setProperty("LoginServerID", "23");
    auto* previous = de::kernelContext().exchangeConfig(&config);
    de::loginContext().setGameServerGroupInfoManager(&groups);
    int result = 0;
    try {
        Session session;
        session.actions = de::defaultLoginRegistrationActions();
        if (!session.registerPlayer() || session.accounts.insertedAccounts.size() != 1 ||
            session.accounts.markedLoggedOnAfterRegister.size() != 1)
            result = 1;
        else {
            const auto& row = session.accounts.insertedAccounts.front();
            const auto& acquisition = session.accounts.markedLoggedOnAfterRegister.front();
            if (!de::password::isHashed(row.password) ||
                de::password::verify(row.password, session.packet.getPassword()) != de::password::Verify::Accepted ||
                acquisition.ip != "198.51.100.7" || acquisition.loginServerID != 23 ||
                session.player.bufferedBytes().substr(szPacketHeader) != std::string("\5Seven\1", 7))
                result = 2;
        }
        const auto saved = session.actions.groupName(7, 9);
        rows.name = "Changed";
        groups.load(rows);
        if (saved != "Seven" || session.actions.groupName(7, 9) != "Changed")
            result = 3;
        try {
            (void)session.actions.groupName(7, 1);
            result = 4;
        } catch (const NoSuchElementException&) {
        }
    } catch (...) {
        result = 5;
    }
    de::loginContext().setGameServerGroupInfoManager(nullptr);
    de::kernelContext().exchangeConfig(previous);
    return result;
}

TEST(LoginRegistration, ProductionAdaptersUseArgon2ConfigurationCatalogueReloadsAndRealOutput) {
    ASSERT_EXIT(std::_Exit(checkProductionAdapters()), ::testing::ExitedWithCode(0), "");
}

class RegistrationLogs {
public:
    RegistrationLogs() {
        if (!mkdtemp(directory) || chdir(directory) != 0)
            std::_Exit(90);
    }
    ~RegistrationLogs() {
        unlink("loginfail.txt");
        chdir("/");
        rmdir(directory);
    }

private:
    char directory[64] = "/tmp/darkeden-registration-XXXXXX";
};

int checkHashingDiagnostic(std::size_t position) {
    RegistrationLogs logs;
    Session session;
    session.actions.decision.hashPassword = [](const std::string&) -> std::string {
        throw std::runtime_error("synthetic hashing failure");
    };
    const auto& production = de::defaultLoginRegistrationActions();
    bool injected = false;
    session.actions.hashingFailure = [&](const std::string& account, const std::string& detail) {
        AllocationProbe probe(position);
        try {
            production.hashingFailure(account, detail);
        } catch (...) {
            injected = probe.rejected();
            throw;
        }
        injected = probe.rejected();
    };
    bool disconnected = false;
    try {
        (void)session.registerPlayer();
    } catch (const DisconnectException& error) {
        disconnected = error.getMessage() == "password hashing failed";
    }
    if (!disconnected || session.refusals != std::vector<BYTE>{15} || session.player.getFailureCount() != 0 ||
        (position == 1 && !injected))
        return 1;
    if (position == 0) {
        std::ifstream log("loginfail.txt");
        std::string line;
        if (!std::getline(log, line) ||
            !line.ends_with(" : Password hashing failed, PlayerID : newcomer : synthetic hashing failure") ||
            std::getline(log, line))
            return 2;
    }
    return 0;
}

TEST(LoginRegistration, ProductionHashingDiagnosticsPreserveTheirTextAndCannotReplaceTheDisconnect) {
    for (std::size_t position = 0; position <= 16; ++position) {
        SCOPED_TRACE(position);
        ASSERT_EXIT(std::_Exit(checkHashingDiagnostic(position)), ::testing::ExitedWithCode(0), "");
    }
}

int checkAllocationFailure(std::size_t position, int mode) {
    auto session = std::make_unique<Session>();
    session->packet.setName(std::string(maxNameLength, 'n'));
    session->packet.setAddress(std::string(maxAddressLength, 'a'));
    session->packet.setEmail(std::string(maxEmailLength, 'e'));
    session->packet.setHomepage(std::string(maxHomepageLength, 'h'));
    session->packet.setProfile(std::string(maxProfileLength, 'p'));
    if (mode == 1)
        session->accounts.registeredIDs.insert("newcomer");
    if (mode == 2)
        session->accounts.locationFound = false;
    if (mode == 3 || mode == 4) {
        session->accounts.failureStage = mode == 3 ? Stage::Exists : Stage::Acquire;
        session->accounts.failure = std::make_exception_ptr(DatabaseError("operation failed"));
    }
    const auto sendFailure = std::make_exception_ptr(std::runtime_error("send failed"));
    if (mode == 6) {
        session->actions.decision.hashPassword = [](const std::string&) -> std::string {
            throw std::runtime_error("synthetic hashing failure");
        };
        session->actions.hashingFailure = [&](const std::string&, const std::string&) {
            std::rethrow_exception(sendFailure);
        };
    }
    session->actions.sendSuccess = [&](LoginPlayer&, LCRegisterPlayerOK& reply) {
        SocketOutputStream output(nullptr, 128);
        reply.write(output);
        if (mode == 5)
            std::rethrow_exception(sendFailure);
        ++session->successes;
    };
    session->actions.sendRefusal = [&](LoginPlayer&, LCRegisterPlayerError& reply) {
        SocketOutputStream output(nullptr, 64);
        reply.write(output);
        session->refusals.push_back(reply.getErrorID());
    };
    AllocationProbe probe(position);
    bool accepted = false;
    bool allocationFailed = false;
    bool hashFailed = false;
    bool sendFailed = false;
    try {
        accepted = session->registerPlayer();
    } catch (const std::bad_alloc&) {
        allocationFailed = true;
    } catch (const DisconnectException& error) {
        hashFailed = error.getMessage() == "password hashing failed";
        if (!hashFailed)
            return 1;
    } catch (...) {
        sendFailed = mode == 5 && std::current_exception() == sendFailure;
        if (!sendFailed)
            return 2;
    }
    probe.stopFailing();
    if ((allocationFailed && !probe.rejected()) || (position == 96 && probe.rejected()) ||
        (!probe.rejected() && (accepted != (mode == 0) || sendFailed != (mode == 5) || hashFailed != (mode == 6))))
        return 3;
    const bool acquired = !session->accounts.markedLoggedOnAfterRegister.empty();
    if (session->player.loginAccountOwnership().owns("newcomer") != acquired ||
        session->successes != (accepted ? 1u : 0u))
        return 4;
    const auto expectedPhase = accepted                                ? LPS_WAITING_FOR_CL_GET_PC_LIST
                               : session->player.getFailureCount() > 0 ? LPS_WAITING_FOR_CL_REGISTER_PLAYER
                                                                       : LPS_BEGIN_SESSION;
    if (session->player.getPlayerStatus() != expectedPhase || session->player.getWorldID() != (accepted ? 7 : 3) ||
        session->player.getServerGroupID() != (accepted ? 9 : 2) ||
        session->player.getID() != (accepted ? "newcomer" : "guest"))
        return 5;
    if (!accepted) {
        session->accounts.failure = nullptr;
        session->accounts.locationFound = true;
        session->actions.decision.hashPassword = [](const std::string&) { return std::string("encoded-test-hash"); };
        session->actions.hashingFailure = {};
        mode = 0;
        const bool canRegister = !session->accounts.registeredIDs.contains("newcomer");
        if (session->registerPlayer() != canRegister)
            return 6;
    }
    session->player.loginAccountOwnership().release(
        [&](const std::string& account) { session->accounts.markLoggedOff(account); });
    session.reset();
    return probe.outstanding() == 0 ? 0 : 7;
}

TEST(LoginRegistration, AllocationFailuresPreservePartialEffectsOwnershipAndCleanupThroughRetry) {
    for (int mode = 0; mode < 7; ++mode) {
        for (std::size_t position = 1; position <= 96; ++position) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << position);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(position, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
