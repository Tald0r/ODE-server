#include <cstdlib>
#include <exception>
#include <functional>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "CLLogin.h"
#include "DatabaseError.h"
#include "LCLoginError.h"
#include "LCLoginOK.h"
#include "LoginFlow.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"

namespace {

class Accounts : public FakeLoginAccountRepository {
public:
    void visit(const std::string& stage) {
        calls.push_back(stage);
        if (observe)
            observe(stage);
        if (stage == failureStage && failure)
            std::rethrow_exception(failure);
    }
    std::vector<LoginIPBlockRow> loadIPBlocks(const std::string& a, const std::string& b,
                                              const std::string& c) override {
        visit("address");
        return FakeLoginAccountRepository::loadIPBlocks(a, b, c);
    }
    bool loadPasswordHash(const std::string& account, std::string& stored) override {
        visit("password:" + account);
        return FakeLoginAccountRepository::loadPasswordHash(account, stored);
    }
    bool loadAccount(const std::string& account, LoginAccountRow& row) override {
        visit("ordinary:" + account);
        return FakeLoginAccountRepository::loadAccount(account, row);
    }
    bool loadAccountForWebLogin(const std::string& account, LoginAccountRow& row) override {
        visit("web:" + account);
        return FakeLoginAccountRepository::loadAccountForWebLogin(account, row);
    }
    bool loadAccountForFreePass(const std::string& account, LoginAccountRow& row) override {
        visit("free:" + account);
        return FakeLoginAccountRepository::loadAccountForFreePass(account, row);
    }
    bool loadWebLoginKey(const std::string& account, std::string& key, std::string& created,
                         std::string& now) override {
        visit("key:" + account);
        return FakeLoginAccountRepository::loadWebLoginKey(account, key, created, now);
    }
    void deleteWebLoginKey(const std::string& account) override {
        visit("consume:" + account);
        FakeLoginAccountRepository::deleteWebLoginKey(account);
        webLoginKeys.erase(account);
    }
    void updatePassword(const std::string& hash, const std::string& account) override {
        visit("rehash:" + account);
        FakeLoginAccountRepository::updatePassword(hash, account);
    }
    void insertTestClientUser(const std::string& account, const std::string& ip) override {
        visit("test:" + account);
        FakeLoginAccountRepository::insertTestClientUser(account, ip);
    }
    bool markLoggedOn(const std::string& ip, int server, const std::string& account) override {
        visit("acquire:" + account);
        return FakeLoginAccountRepository::markLoggedOn(ip, server, account);
    }
    void extendPayPlayByWeek(const std::string& account) override {
        visit("premium:" + account);
        FakeLoginAccountRepository::extendPayPlayByWeek(account);
    }
    void insertLoginRecord(const std::string& account, const std::string& ip, const std::string& date,
                           const std::string& time) override {
        visit("statistics:" + account);
        FakeLoginAccountRepository::insertLoginRecord(account, ip, date, time);
    }
    std::vector<std::string> calls;
    std::function<void(const std::string&)> observe;
    std::string failureStage;
    std::exception_ptr failure;
};

class FlowPlayer : public LoginPlayer {
public:
    FlowPlayer() : LoginPlayer(new Socket("198.51.100.7", 1234)) {
        setID("visitor");
        setFailureCount(0);
        setPlayerStatus(LPS_BEGIN_SESSION);
        setWebLogin(false);
    }
    ~FlowPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
    std::string bufferedBytes() const {
        return {m_pOutputStream->getBuffer(), m_pOutputStream->length()};
    }
};

struct Session {
    Session() : actions(de::defaultLoginFlowActions()) {
        actions.configuration = [&] {
            accounts.visit("configuration");
            return de::LoginFlowConfiguration{7, false};
        };
        actions.clock = [&] {
            accounts.visit("clock");
            return VSDateTime(VSDate(2026, 9, 1), VSTime(12, 0, 0));
        };
        actions.sendSuccess = [&](LoginPlayer& recipient, LCLoginOK& reply) {
            accounts.visit("success");
            EXPECT_EQ(recipient.getPlayerStatus(), LPS_BEGIN_SESSION);
            EXPECT_TRUE(recipient.loginAccountOwnership().owns(recipient.getID()));
            recipient.sendPacket(&reply);
            ++successes;
        };
        actions.kick = [&](LoginPlayer&) { accounts.visit("kick"); };
        actions.authentication.sendRefusal = [&](LoginPlayer& recipient, LCLoginError& reply) {
            accounts.visit("refusal");
            refusals.push_back(reply.getErrorID());
            recipient.sendPacket(&reply);
        };
        actions.authentication.refusal = {};
        actions.authentication.hashingFailure = {};
        actions.authentication.newNetMarbleAccount = {};
        actions.authentication.webKey.mismatch = {};
        actions.authentication.hashPassword = [](std::string_view) { return std::string("replacement-hash"); };
    }
    void account(const std::string& id) {
        accounts.accounts[id] = Accounts::allowedAccount(id);
        accounts.storedPasswords[id] = "correct-password";
    }
    CLLogin ordinary(const std::string& id, const std::string& credential = "correct-password") {
        CLLogin packet;
        packet.setID(id);
        packet.setPassword(credential);
        return packet;
    }
    CLLogin web(const std::string& id) {
        auto packet = ordinary(id, "accepted-key");
        packet.setWebLogin();
        accounts.webLoginKeys[id] = {"accepted-key", "2026-09-01 12:00:00", "2026-09-01 12:01:00"};
        return packet;
    }
    void login(CLLogin& packet) {
        de::loginPlayer(player, packet, accounts, actions);
    }
    Accounts accounts;
    FlowPlayer player;
    de::LoginFlowActions actions;
    std::vector<BYTE> refusals;
    unsigned successes = 0;
};

TEST(LoginFlow, ARefusedWebAccountCannotAuthorizeAnotherAccountsOrdinaryPassword) {
    Session session;
    session.account("web-user");
    session.accounts.accounts["web-user"].access = "DENY";
    auto first = session.web("web-user");
    session.login(first);
    ASSERT_EQ(session.refusals, std::vector<BYTE>{ETC_ERROR});
    ASSERT_EQ(session.accounts.deletedWebLoginKeys, std::vector<std::string>{"web-user"});
    ASSERT_TRUE(session.accounts.webLoginKeys.empty());
    ASSERT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    ASSERT_EQ(session.player.loginAccountOwnership().account(), nullptr);

    session.account("ordinary-user");
    auto second = session.ordinary("ordinary-user", "wrong-password");
    const auto before = session.player.bufferedBytes().size();
    session.login(second);
    EXPECT_EQ(session.accounts.loadPasswordHashCalls, 1);
    EXPECT_EQ(session.accounts.loadAccountCalls, 1);
    EXPECT_EQ(session.accounts.loadAccountForFreePassCalls, 0);
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{ETC_ERROR, INVALID_ID_PASSWORD}));
    EXPECT_EQ(session.player.bufferedBytes().substr(before + szPacketHeader), std::string(1, INVALID_ID_PASSWORD));
    EXPECT_EQ(session.successes, 0u);
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    EXPECT_EQ(session.player.loginAccountOwnership().account(), nullptr);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
}

TEST(LoginFlow, OrdinaryPasswordMigrationPrecedesAcquisitionAndReplyPrecedesPublication) {
    Session session;
    session.account("rowan");
    auto packet = session.ordinary("  rowan  ");
    session.login(packet);
    EXPECT_EQ(packet.getID(), "rowan");
    EXPECT_EQ(session.accounts.calls,
              (std::vector<std::string>{"address", "configuration", "password:rowan", "rehash:rowan", "clock",
                                        "ordinary:rowan", "acquire:rowan", "success", "clock", "statistics:rowan"}));
    EXPECT_EQ(session.successes, 1u);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("rowan"));
    EXPECT_EQ(session.accounts.markedLoggedOn.front().loginServerID, 7);
    EXPECT_EQ(session.accounts.markedLoggedOn.front().ip, "198.51.100.7");
}

TEST(LoginFlow, WebAcceptanceConsumesItsKeyBeforeProjectionAndNeverChecksAnOrdinaryPassword) {
    Session session;
    session.account("rowan");
    auto packet = session.web("rowan");
    session.login(packet);
    EXPECT_EQ(session.accounts.calls,
              (std::vector<std::string>{"address", "key:rowan", "consume:rowan", "configuration", "clock", "web:rowan",
                                        "acquire:rowan", "success", "clock", "statistics:rowan"}));
    EXPECT_EQ(session.accounts.loadPasswordHashCalls, 0);
    EXPECT_EQ(session.successes, 1u);
}

TEST(LoginFlow, TestClientPrefixesRetainTheirAuthenticationAndRecordingOrder) {
    for (bool netMarble : {false, true}) {
        Session session;
        session.account("rowan");
        session.accounts.storedPasswords["@rowan"] = "correct-password";
        auto packet = session.ordinary(netMarble ? " #@rowan " : " #rowan ");
        packet.setNetmarble(netMarble);
        session.login(packet);
        EXPECT_EQ(packet.getID(), "rowan");
        EXPECT_EQ(session.accounts.testClientUsers,
                  (std::vector<std::pair<std::string, std::string>>{{"rowan", "198.51.100.7"}}));
        EXPECT_EQ(session.accounts.calls[0], "address");
        if (netMarble) {
            EXPECT_EQ(session.accounts.calls[1], "password:@rowan");
            EXPECT_EQ(session.accounts.calls[2], "rehash:@rowan");
            EXPECT_EQ(session.accounts.calls[3], "test:rowan");
            EXPECT_EQ(session.accounts.loadAccountForFreePassCalls, 1);
        } else {
            EXPECT_EQ(session.accounts.calls[1], "test:rowan");
            EXPECT_EQ(session.accounts.calls[2], "configuration");
            EXPECT_EQ(session.accounts.calls[3], "password:rowan");
        }
        EXPECT_EQ(session.successes, 1u);
    }
}

TEST(LoginFlow, MissingNetMarbleAccountRetainsBothRefusals) {
    Session session;
    session.accounts.storedPasswords["missing"] = "correct-password";
    auto packet = session.ordinary("missing");
    packet.setNetmarble(true);
    session.login(packet);
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{ETC_ERROR, ETC_ERROR}));
    const auto wire = session.player.bufferedBytes();
    EXPECT_EQ(wire.size(), 2 * (szPacketHeader + 1));
    EXPECT_EQ(wire[szPacketHeader], ETC_ERROR);
    EXPECT_EQ(wire[2 * szPacketHeader + 1], ETC_ERROR);
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
}

TEST(LoginFlow, AddressRefusalPrecedesTestClientAndAuthenticationGates) {
    Session session;
    LoginIPBlockRow block{};
    block.ipClass = 3;
    session.accounts.ipBlocks.push_back(block);
    auto packet = session.web("#rowan");
    session.login(packet);
    EXPECT_EQ(session.accounts.calls, (std::vector<std::string>{"address", "refusal"}));
    EXPECT_EQ(session.refusals, std::vector<BYTE>{IP_DENYED});
    EXPECT_TRUE(session.accounts.deletedWebLoginKeys.empty());
    EXPECT_TRUE(session.accounts.testClientUsers.empty());
}

TEST(LoginFlow, KickDispatchOwnsCompletionAndReceivesThePublishedIdentity) {
    Session session;
    session.account("rowan");
    auto& row = session.accounts.accounts["rowan"];
    row.logOn = "GAME";
    row.loginIP = "198.51.100.7";
    row.ssn = "800101-1234567";
    row.zipCode = "123-456";
    session.actions.kick = [&](LoginPlayer& player) {
        EXPECT_EQ(player.getID(), "rowan");
        EXPECT_EQ(player.getSSN(), row.ssn);
        EXPECT_EQ(player.getZipcode(), row.zipCode);
        EXPECT_TRUE(player.isAdult());
        session.accounts.visit("kick");
    };
    auto packet = session.ordinary("rowan");
    session.login(packet);
    EXPECT_EQ(session.accounts.calls.back(), "kick");
    EXPECT_EQ(session.successes, 0u);
    EXPECT_TRUE(session.accounts.loginRecords.empty());
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    EXPECT_EQ(session.player.loginAccountOwnership().account(), nullptr);
}

TEST(LoginFlow, FailuresAfterAcquisitionRetainTheAccountOwnerAndEarlierEffects) {
    for (const std::string stage : {"premium:rowan", "success", "statistics:rowan"}) {
        Session session;
        session.account("rowan");
        session.accounts.unclaimedPremiumEvents.insert("rowan");
        session.accounts.failureStage = stage;
        session.accounts.failure = std::make_exception_ptr(std::runtime_error("operation failed"));
        auto packet = session.ordinary("rowan");
        EXPECT_THROW(session.login(packet), std::runtime_error);
        EXPECT_TRUE(session.player.loginAccountOwnership().owns("rowan"));
        EXPECT_EQ(session.accounts.markedLoggedOn.size(), 1u);
        EXPECT_EQ(session.player.getID(), "rowan");
        EXPECT_EQ(session.player.getPlayerStatus(),
                  stage == "statistics:rowan" ? LPS_WAITING_FOR_CL_GET_PC_LIST : LPS_BEGIN_SESSION);
        EXPECT_EQ(session.successes, stage == "statistics:rowan" ? 1u : 0u);
    }
}

TEST(LoginFlow, DatabaseErrorsKeepTheGateDecisionAndStatisticsBoundaries) {
    for (const std::string stage : {"address", "test:rowan", "configuration", "password:rowan", "ordinary:rowan",
                                    "success", "statistics:rowan"}) {
        Session session;
        session.account("rowan");
        session.accounts.failureStage = stage;
        session.accounts.failure = std::make_exception_ptr(DatabaseError("operation failed"));
        auto packet = session.ordinary("#rowan");
        if (stage == "address" || stage == "test:rowan" || stage == "statistics:rowan") {
            EXPECT_THROW(session.login(packet), DatabaseError);
        } else {
            try {
                session.login(packet);
                FAIL() << "database failure was swallowed";
            } catch (const Error& error) {
                EXPECT_NE(error.toString().find("CLLoginHandler : operation failed"), std::string::npos);
            }
        }
    }
}

TEST(LoginFlow, CallbacksCannotReplaceTheOwnedPacketCredential) {
    Session session;
    session.account("rowan");
    auto packet = session.ordinary("rowan");
    session.accounts.observe = [&](const std::string& stage) {
        if (stage == "address") {
            packet.setID("another");
            packet.setPassword("wrong-password");
        }
    };
    session.login(packet);
    EXPECT_EQ(session.successes, 1u);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("rowan"));
    EXPECT_EQ(session.accounts.markedLoggedOn.front().playerID, "rowan");
}

TEST(LoginFlow, NetMarbleRefusalSendIOExceptionRetainsTheLegacyGateContinuation) {
    Session session;
    session.account("rowan");
    auto packet = session.ordinary("rowan", "wrong-password");
    packet.setNetmarble(true);
    const auto send = session.actions.authentication.sendRefusal;
    unsigned sends = 0;
    session.actions.authentication.sendRefusal = [&](LoginPlayer& player, LCLoginError& reply) {
        if (++sends == 1)
            throw IOException("refusal output failed");
        send(player, reply);
    };
    EXPECT_NO_THROW(session.login(packet));
    EXPECT_EQ(sends, 2u);
    EXPECT_EQ(session.accounts.loadPasswordHashCalls, 2);
    EXPECT_EQ(session.accounts.loadAccountCalls, 1);
    EXPECT_EQ(session.refusals, std::vector<BYTE>{INVALID_ID_PASSWORD});
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
}

TEST(LoginFlow, NetMarbleRefusalProtocolAndErrorFailuresStopBeforeOrdinaryChecking) {
    for (const auto& failure : {std::make_exception_ptr(ProtocolException("refusal protocol failed")),
                                std::make_exception_ptr(Error("refusal failed"))}) {
        Session session;
        session.account("rowan");
        auto packet = session.ordinary("rowan", "wrong-password");
        packet.setNetmarble(true);
        session.actions.authentication.sendRefusal = [&](LoginPlayer&, LCLoginError&) {
            std::rethrow_exception(failure);
        };
        try {
            session.login(packet);
            FAIL() << "gate failure was swallowed";
        } catch (const ProtocolException&) {
        } catch (const Error&) {
        }
        EXPECT_EQ(session.accounts.loadPasswordHashCalls, 1);
        EXPECT_EQ(session.accounts.loadAccountCalls, 0);
        EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    }
}

TEST(LoginFlow, ARefusedNetMarbleAccountCannotAuthorizeAnotherAccountsOrdinaryPassword) {
    Session session;
    session.account("net-user");
    session.accounts.accounts["net-user"].access = "DENY";
    auto first = session.ordinary("net-user");
    first.setNetmarble(true);
    session.login(first);
    ASSERT_EQ(session.refusals, std::vector<BYTE>{ETC_ERROR});
    ASSERT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    ASSERT_EQ(session.player.loginAccountOwnership().account(), nullptr);

    session.account("ordinary-user");
    auto second = session.ordinary("ordinary-user", "wrong-password");
    session.login(second);
    EXPECT_EQ(session.accounts.loadPasswordHashCalls, 2);
    EXPECT_EQ(session.accounts.loadAccountCalls, 1);
    EXPECT_EQ(session.accounts.loadAccountForFreePassCalls, 1);
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{ETC_ERROR, INVALID_ID_PASSWORD}));
    EXPECT_EQ(session.successes, 0u);
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    EXPECT_EQ(session.player.loginAccountOwnership().account(), nullptr);
}

TEST(LoginFlow, APreviousWebGrantCannotStripAnOrdinaryTestClientsAccountPrefix) {
    Session session;
    session.account("web-user");
    session.accounts.accounts["web-user"].access = "DENY";
    auto first = session.web("web-user");
    session.login(first);
    ASSERT_EQ(session.refusals, std::vector<BYTE>{ETC_ERROR});
    session.account("rowan");
    session.account("owan");
    auto second = session.ordinary("#rowan", "wrong-password");
    session.login(second);
    EXPECT_EQ(second.getID(), "rowan");
    EXPECT_EQ(session.accounts.testClientUsers,
              (std::vector<std::pair<std::string, std::string>>{{"rowan", "198.51.100.7"}}));
    EXPECT_EQ(session.accounts.loadPasswordHashCalls, 1);
    EXPECT_EQ(session.accounts.loadAccountForFreePassCalls, 0);
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{ETC_ERROR, INVALID_ID_PASSWORD}));
    EXPECT_EQ(session.successes, 0u);
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
}

TEST(LoginFlow, AnUnrecognizedAccountStateRecordsTheLoginWithoutAReplyOrAcquisition) {
    Session session;
    session.account("rowan");
    session.accounts.accounts["rowan"].logOn = "OTHER";
    auto packet = session.ordinary("rowan");
    session.login(packet);
    EXPECT_TRUE(session.refusals.empty());
    EXPECT_EQ(session.successes, 0u);
    EXPECT_TRUE(session.player.bufferedBytes().empty());
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    EXPECT_EQ(session.accounts.loginRecords, std::vector<std::string>{"rowan"});
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
}

TEST(LoginFlow, FailureLimitDisconnectsOnlyAfterTheRefusalWasSent) {
    for (unsigned failures : {3u, 4u}) {
        Session session;
        session.account("rowan");
        session.player.setFailureCount(failures);
        auto packet = session.ordinary("rowan", "wrong-password");
        if (failures > 3) {
            EXPECT_THROW(session.login(packet), DisconnectException);
        } else {
            EXPECT_NO_THROW(session.login(packet));
        }
        EXPECT_EQ(session.refusals, std::vector<BYTE>{INVALID_ID_PASSWORD});
        EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), std::string(1, INVALID_ID_PASSWORD));
        EXPECT_EQ(session.player.getFailureCount(), failures);
        EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
        EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    }
}

TEST(LoginFlow, AWebGrantCannotAuthorizeAnOrdinaryRetryOfTheSameAccount) {
    Session session;
    session.account("rowan");
    session.accounts.accounts["rowan"].access = "DENY";
    auto first = session.web("rowan");
    session.login(first);
    ASSERT_EQ(session.refusals, std::vector<BYTE>{ETC_ERROR});
    session.accounts.accounts["rowan"].access = "ALLOW";
    auto second = session.ordinary("rowan", "wrong-password");
    session.login(second);
    EXPECT_FALSE(session.player.isFreePass());
    EXPECT_FALSE(session.player.isWebLogin());
    EXPECT_EQ(session.accounts.loadPasswordHashCalls, 1);
    EXPECT_EQ(session.refusals, (std::vector<BYTE>{ETC_ERROR, INVALID_ID_PASSWORD}));
    EXPECT_EQ(session.successes, 0u);
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
}

TEST(LoginFlow, FailedExternalAttemptsClearPriorGrantsBeforeAnOrdinaryRetry) {
    for (bool web : {false, true}) {
        Session session;
        session.account("first");
        session.accounts.accounts["first"].access = "DENY";
        auto first = session.web("first");
        session.login(first);
        ASSERT_TRUE(session.player.isFreePass());

        session.account("rejected");
        auto failed = session.ordinary("rejected", "wrong-password");
        if (web)
            failed.setWebLogin();
        else
            failed.setNetmarble(true);
        session.login(failed);
        EXPECT_FALSE(session.player.isFreePass());
        EXPECT_EQ(session.player.isWebLogin(), web);
        ASSERT_EQ(session.refusals.size(), 2u);
        EXPECT_EQ(session.refusals.back(), web ? NOT_FOUND_KEY : INVALID_ID_PASSWORD);

        session.account("ordinary");
        auto last = session.ordinary("ordinary", "wrong-password");
        session.login(last);
        EXPECT_FALSE(session.player.isFreePass());
        EXPECT_FALSE(session.player.isWebLogin());
        EXPECT_EQ(session.accounts.loadAccountForFreePassCalls, 0);
        EXPECT_EQ(session.accounts.loadAccountCalls, 1);
        ASSERT_EQ(session.refusals.size(), 3u);
        EXPECT_EQ(session.refusals.back(), INVALID_ID_PASSWORD);
        EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    }
}

TEST(LoginFlow, CorrectOrdinaryPasswordStillAcquiresAfterARefusedWebAttempt) {
    Session session;
    session.account("first");
    session.accounts.accounts["first"].access = "DENY";
    auto first = session.web("first");
    session.login(first);
    session.account("ordinary");
    auto second = session.ordinary("ordinary");
    session.login(second);
    EXPECT_FALSE(session.player.isFreePass());
    EXPECT_FALSE(session.player.isWebLogin());
    EXPECT_EQ(session.accounts.loadPasswordHashCalls, 1);
    EXPECT_EQ(session.accounts.loadAccountCalls, 1);
    EXPECT_EQ(session.accounts.loadAccountForFreePassCalls, 0);
    EXPECT_EQ(session.successes, 1u);
    EXPECT_TRUE(session.player.loginAccountOwnership().owns("ordinary"));
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
}

TEST(LoginFlow, ANewExternalAuthenticationCanGrantItsOwnAuthorizationAfterRefusal) {
    for (bool web : {false, true}) {
        Session session;
        session.account("first");
        session.accounts.accounts["first"].access = "DENY";
        auto first = session.web("first");
        session.login(first);
        session.account("accepted");
        auto second = web ? session.web("accepted") : session.ordinary("accepted");
        second.setNetmarble(!web);
        session.login(second);
        EXPECT_TRUE(session.player.isFreePass());
        EXPECT_EQ(session.player.isWebLogin(), web);
        EXPECT_EQ(session.successes, 1u);
        EXPECT_TRUE(session.player.loginAccountOwnership().owns("accepted"));
        EXPECT_EQ(session.accounts.loadPasswordHashCalls, web ? 0 : 1);
        EXPECT_EQ(session.accounts.loadAccountCalls, 0);
        EXPECT_EQ(session.accounts.loadAccountForFreePassCalls, web ? 0 : 1);
    }
}

TEST(LoginFlow, ANewAttemptCannotDiscardOrReplaceAnExistingAccountCleanupOwner) {
    for (bool correct : {false, true}) {
        Session session;
        ASSERT_TRUE(session.player.loginAccountOwnership().acquire("owned", [] { return true; }));
        session.player.setFreePass(true);
        session.player.setWebLogin(true);
        session.account("ordinary");
        auto packet = session.ordinary("ordinary", correct ? "correct-password" : "wrong-password");
        if (correct) {
            EXPECT_THROW(session.login(packet), DisconnectException);
        } else {
            EXPECT_NO_THROW(session.login(packet));
        }
        EXPECT_FALSE(session.player.isFreePass());
        EXPECT_FALSE(session.player.isWebLogin());
        EXPECT_TRUE(session.player.loginAccountOwnership().owns("owned"));
        EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
        EXPECT_EQ(session.successes, 0u);
    }
}

int failedInputSnapshotClearsAuthorization() {
    Session session;
    session.player.loginAccountOwnership().acquire("owned", [] { return true; });
    session.player.setFreePass(true);
    session.player.setWebLogin(true);
    session.account("ordinary");
    auto packet = session.ordinary("ordinary", std::string(30, 'x'));
    {
        AllocationProbe probe(1);
        bool failed = false;
        try {
            session.login(packet);
        } catch (const std::bad_alloc&) {
            failed = true;
        } catch (...) {
            return 1;
        }
        probe.stopFailing();
        if (!failed || !probe.rejected() || probe.outstanding() != 0 || session.player.isFreePass() ||
            session.player.isWebLogin() || !session.player.loginAccountOwnership().owns("owned"))
            return 2;
    }
    packet.setPassword("wrong-password");
    session.login(packet);
    return session.refusals == std::vector<BYTE>{INVALID_ID_PASSWORD} && session.accounts.markedLoggedOn.empty() &&
                   session.player.loginAccountOwnership().owns("owned")
               ? 0
               : 3;
}

TEST(LoginFlow, InputAllocationFailureClearsAuthorizationAndPreservesCleanupThroughRetry) {
    ASSERT_EXIT(std::_Exit(failedInputSnapshotClearsAuthorization()), ::testing::ExitedWithCode(0), "");
}

} // namespace
