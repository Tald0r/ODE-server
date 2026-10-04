#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "CLLogin.h"
#include "DatabaseError.h"
#include "LCLoginError.h"
#include "LoginAuthentication.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"

namespace {

enum class Stage {
    Stored,
    Verify,
    Hash,
    Update,
    NewReport,
    Insert,
    Web,
    Delete,
    Send,
    RefusalReport,
    HashReport,
    MismatchReport
};

class Accounts : public FakeLoginAccountRepository {
public:
    void visit(Stage stage) {
        calls.push_back(stage);
        if (before)
            before(stage);
        if (failure && failureStage == stage)
            std::rethrow_exception(failure);
    }
    bool loadPasswordHash(const std::string& account, std::string& hash) override {
        visit(Stage::Stored);
        queries.push_back(account);
        return FakeLoginAccountRepository::loadPasswordHash(account, hash);
    }
    void updatePassword(const std::string& hash, const std::string& account) override {
        visit(Stage::Update);
        auto writes = updatedPasswords;
        auto rows = storedPasswords;
        writes.emplace_back(hash, account);
        rows[account] = hash;
        updatedPasswords.swap(writes);
        storedPasswords.swap(rows);
        if (after)
            after(Stage::Update);
    }
    void insertNetMarbleAccount(const std::string& account, const std::string& hash) override {
        visit(Stage::Insert);
        auto writes = netMarbleAccounts;
        auto rows = storedPasswords;
        writes.emplace_back(account, hash);
        rows.emplace(account, hash);
        netMarbleAccounts.swap(writes);
        storedPasswords.swap(rows);
        if (after)
            after(Stage::Insert);
    }
    bool loadWebLoginKey(const std::string& account, std::string& key, std::string& created,
                         std::string& now) override {
        visit(Stage::Web);
        queries.push_back(account);
        return FakeLoginAccountRepository::loadWebLoginKey(account, key, created, now);
    }
    void deleteWebLoginKey(const std::string& account) override {
        visit(Stage::Delete);
        auto writes = deletedWebLoginKeys;
        writes.push_back(account);
        deletedWebLoginKeys.swap(writes);
        webLoginKeys.erase(account);
        if (after)
            after(Stage::Delete);
    }
    std::vector<Stage> calls;
    std::vector<std::string> queries;
    std::function<void(Stage)> before;
    std::function<void(Stage)> after;
    Stage failureStage = Stage::Stored;
    std::exception_ptr failure;
};

class AuthenticationPlayer : public LoginPlayer {
public:
    AuthenticationPlayer() : LoginPlayer(new Socket("198.51.100.7", 1234)) {
        setID("visitor");
        setWorldID(7);
        setServerGroupID(9);
        setFailureCount(2);
        setPlayerStatus(LPS_BEGIN_SESSION);
        setFreePass(false);
        setWebLogin(false);
    }
    ~AuthenticationPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
    std::string bufferedBytes() const {
        return {m_pOutputStream->getBuffer(), m_pOutputStream->length()};
    }
};

struct Session {
    Session() {
        packet.setID("Rowan");
        packet.setPassword("test-credential");
        packet.setNetmarble(true);
    }
    void validKey() {
        accounts.webLoginKeys["Rowan"] = {"test-credential", "2026-09-01 12:00:00", "2026-09-01 12:05:00"};
    }
    bool netMarble() {
        return de::authorizeNetMarbleLogin(player, packet, accounts, actions);
    }
    bool web() {
        return de::authorizeWebLogin(player, packet, accounts, actions);
    }
    Accounts accounts;
    AuthenticationPlayer player;
    CLLogin packet;
    de::password::Verify verdict = de::password::Verify::Accepted;
    std::vector<std::pair<std::string, std::string>> verifications;
    std::vector<std::string> hashes;
    std::vector<BYTE> replies;
    std::vector<std::tuple<std::string, std::string, int>> refusals;
    std::vector<std::tuple<std::string, std::string, bool>> hashFailures;
    std::vector<std::string> newAccounts;
    std::vector<std::string> mismatches;
    de::LoginAuthenticationActions actions{[&](std::string_view stored, std::string_view credential) {
                                               accounts.visit(Stage::Verify);
                                               verifications.emplace_back(stored, credential);
                                               return verdict;
                                           },
                                           [&](std::string_view credential) {
                                               accounts.visit(Stage::Hash);
                                               hashes.emplace_back(credential);
                                               return std::string("encoded-test-hash");
                                           },
                                           [&](LoginPlayer&, LCLoginError& reply) {
                                               accounts.visit(Stage::Send);
                                               replies.push_back(reply.getErrorID());
                                           },
                                           [&](const std::string& account, const char* name, int site) {
                                               accounts.visit(Stage::RefusalReport);
                                               refusals.emplace_back(account, name, site);
                                           },
                                           [&](const std::string& account, const char* detail, bool rehash) {
                                               accounts.visit(Stage::HashReport);
                                               hashFailures.emplace_back(account, detail, rehash);
                                           },
                                           [&](const std::string& account) {
                                               accounts.visit(Stage::NewReport);
                                               newAccounts.push_back(account);
                                           },
                                           {[&](const std::string& account) {
                                               accounts.visit(Stage::MismatchReport);
                                               mismatches.push_back(account);
                                           }}};
};

void expectSessionUnchanged(const Session& session) {
    EXPECT_EQ(session.player.getID(), "visitor");
    EXPECT_EQ(session.player.getWorldID(), 7);
    EXPECT_EQ(session.player.getServerGroupID(), 9);
    EXPECT_EQ(session.player.getFailureCount(), 2u);
    EXPECT_EQ(session.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_FALSE(session.player.isWebLogin());
    EXPECT_EQ(session.player.loginAccountOwnership().account(), nullptr);
    EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
}

TEST(LoginAuthentication, AFailedWebKeyDeletionCannotGrantAFreePass) {
    Session session;
    session.validKey();
    session.accounts.failureStage = Stage::Delete;
    session.accounts.failure = std::make_exception_ptr(Error("delete failed"));
    EXPECT_FALSE(session.web());
    EXPECT_FALSE(session.player.isFreePass());
    EXPECT_TRUE(session.accounts.webLoginKeys.contains("Rowan"));
}

TEST(LoginAuthentication, RehashDiagnosticFailureCannotRejectAnAlreadyVerifiedPassword) {
    Session session;
    session.accounts.storedPasswords["Rowan"] = "legacy";
    session.verdict = de::password::Verify::AcceptedRehash;
    session.actions.hashPassword = [](std::string_view) -> std::string { throw std::runtime_error("hash failed"); };
    session.accounts.failureStage = Stage::HashReport;
    session.accounts.failure = std::make_exception_ptr(DatabaseError("diagnostic failed"));
    EXPECT_NO_THROW(EXPECT_TRUE(session.netMarble()));
    EXPECT_TRUE(session.player.isFreePass());
    EXPECT_TRUE(session.accounts.updatedPasswords.empty());
}

TEST(LoginAuthentication, NewAccountReportingCannotPreventAuthenticationOrInsertion) {
    Session session;
    session.accounts.failureStage = Stage::NewReport;
    session.accounts.failure = std::make_exception_ptr(Error("diagnostic failed"));
    EXPECT_TRUE(session.netMarble());
    EXPECT_TRUE(session.player.isFreePass());
    ASSERT_EQ(session.accounts.netMarbleAccounts.size(), 1u);
}

TEST(LoginAuthentication, RefusalDiagnosticsCannotReplaceACompletedReply) {
    Session session;
    session.accounts.failureStage = Stage::RefusalReport;
    session.accounts.failure = std::make_exception_ptr(std::runtime_error("diagnostic failed"));
    EXPECT_NO_THROW(de::sendLoginRefusal(session.player, "Rowan", LoginRejectReason::MalformedID, session.actions));
    EXPECT_EQ(session.replies, (std::vector<BYTE>{0}));
}

TEST(LoginAuthentication, MismatchDiagnosticsCannotSuppressTheWebRefusal) {
    Session session;
    session.validKey();
    session.packet.setPassword("different");
    session.accounts.failureStage = Stage::MismatchReport;
    session.accounts.failure = std::make_exception_ptr(Error("diagnostic failed"));
    EXPECT_FALSE(session.web());
    EXPECT_EQ(session.replies, (std::vector<BYTE>{0}));
    EXPECT_EQ(session.refusals.size(), 1u);
    EXPECT_FALSE(session.player.isFreePass());
}

TEST(LoginAuthentication, OrdinaryClientsBypassTheNetMarbleGateWithoutDependenciesOrStateChanges) {
    for (bool freePass : {false, true}) {
        Session session;
        session.packet.setNetmarble(false);
        session.player.setFreePass(freePass);
        session.actions = {};
        EXPECT_TRUE(session.netMarble());
        EXPECT_EQ(session.player.isFreePass(), freePass);
        EXPECT_TRUE(session.accounts.calls.empty());
        expectSessionUnchanged(session);
    }
}

TEST(LoginAuthentication, ExistingPasswordsVerifyOnceAndOnlyAcceptedRehashWrites) {
    for (const auto verdict :
         {de::password::Verify::Rejected, de::password::Verify::Accepted, de::password::Verify::AcceptedRehash}) {
        Session session;
        session.accounts.storedPasswords["Rowan"] = "stored-credential";
        session.verdict = verdict;
        const bool accepted = verdict != de::password::Verify::Rejected;
        session.accounts.before = [&](Stage stage) {
            if (stage == Stage::Update)
                EXPECT_FALSE(session.player.isFreePass());
        };
        EXPECT_EQ(session.netMarble(), accepted);
        EXPECT_EQ(session.player.isFreePass(), accepted);
        EXPECT_EQ(session.verifications,
                  (std::vector<std::pair<std::string, std::string>>{{"stored-credential", "test-credential"}}));
        EXPECT_EQ(session.accounts.queries, (std::vector<std::string>{"Rowan"}));
        if (verdict == de::password::Verify::AcceptedRehash) {
            EXPECT_EQ(session.accounts.calls,
                      (std::vector<Stage>{Stage::Stored, Stage::Verify, Stage::Hash, Stage::Update}));
            EXPECT_EQ(session.hashes, (std::vector<std::string>{"test-credential"}));
            EXPECT_EQ(session.accounts.updatedPasswords,
                      (std::vector<std::pair<std::string, std::string>>{{"encoded-test-hash", "Rowan"}}));
        } else {
            EXPECT_TRUE(session.hashes.empty());
            EXPECT_TRUE(session.accounts.updatedPasswords.empty());
            if (accepted) {
                EXPECT_EQ(session.accounts.calls, (std::vector<Stage>{Stage::Stored, Stage::Verify}));
                EXPECT_TRUE(session.replies.empty());
            } else {
                EXPECT_EQ(session.accounts.calls,
                          (std::vector<Stage>{Stage::Stored, Stage::Verify, Stage::Send, Stage::RefusalReport}));
                EXPECT_EQ(session.replies, (std::vector<BYTE>{0}));
                EXPECT_EQ(session.refusals, (std::vector<std::tuple<std::string, std::string, int>>{
                                                {"Rowan", "INVALID_ID_PASSWORD", 9}}));
            }
        }
        EXPECT_TRUE(session.accounts.netMarbleAccounts.empty());
        expectSessionUnchanged(session);
    }
}

TEST(LoginAuthentication, NewNetMarbleAccountsHashAndInsertBeforeGrantingTheFreePass) {
    Session session;
    session.accounts.after = [&](Stage stage) {
        if (stage == Stage::Insert)
            EXPECT_FALSE(session.player.isFreePass());
    };
    EXPECT_TRUE(session.netMarble());
    EXPECT_EQ(session.accounts.calls,
              (std::vector<Stage>{Stage::Stored, Stage::NewReport, Stage::Hash, Stage::Insert}));
    EXPECT_EQ(session.newAccounts, (std::vector<std::string>{"Rowan"}));
    EXPECT_EQ(session.hashes, (std::vector<std::string>{"test-credential"}));
    EXPECT_EQ(session.accounts.netMarbleAccounts,
              (std::vector<std::pair<std::string, std::string>>{{"Rowan", "encoded-test-hash"}}));
    EXPECT_TRUE(session.verifications.empty());
    EXPECT_TRUE(session.replies.empty());
    EXPECT_TRUE(session.player.isFreePass());
    expectSessionUnchanged(session);
}

TEST(LoginAuthentication, StandaloneNetMarbleVerificationDoesNotPublishSessionStateOrReply) {
    Session session;
    EXPECT_TRUE(de::verifyNetMarblePassword(session.packet, session.accounts, session.actions));
    EXPECT_FALSE(session.player.isFreePass());
    EXPECT_TRUE(session.replies.empty());
    EXPECT_EQ(session.accounts.netMarbleAccounts.size(), 1u);
    expectSessionUnchanged(session);
}

TEST(LoginAuthentication, StandardHashFailuresKeepAcceptedPasswordsAndRefuseNewAccounts) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(std::runtime_error("hash failed")),
                                           std::make_exception_ptr(Error("hash failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (bool existing : {false, true}) {
        for (const auto& failure : failures) {
            Session session;
            if (existing)
                session.accounts.storedPasswords["Rowan"] = "legacy";
            session.verdict = de::password::Verify::AcceptedRehash;
            session.accounts.failureStage = Stage::Hash;
            session.accounts.failure = failure;
            EXPECT_EQ(session.netMarble(), existing);
            EXPECT_EQ(session.player.isFreePass(), existing);
            ASSERT_EQ(session.hashFailures.size(), 1u);
            EXPECT_EQ(std::get<0>(session.hashFailures.front()), "Rowan");
            EXPECT_EQ(std::get<2>(session.hashFailures.front()), existing);
            EXPECT_TRUE(session.accounts.updatedPasswords.empty());
            EXPECT_TRUE(session.accounts.netMarbleAccounts.empty());
            EXPECT_EQ(session.replies.size(), existing ? 0u : 1u);
            expectSessionUnchanged(session);
        }
    }
}

TEST(LoginAuthentication, NetMarbleOperationExceptionsKeepTheirHistoricalBoundary) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(Error("operation failed")),
                                           std::make_exception_ptr(InvalidProtocolException("operation failed")),
                                           std::make_exception_ptr(DatabaseError("operation failed")),
                                           std::make_exception_ptr(std::runtime_error("operation failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto stage : {Stage::Stored, Stage::Verify, Stage::Update, Stage::Insert}) {
        for (std::size_t kind = 0; kind < std::size(failures); ++kind) {
            Session session;
            if (stage != Stage::Insert)
                session.accounts.storedPasswords["Rowan"] = "stored";
            session.verdict = de::password::Verify::AcceptedRehash;
            session.accounts.failureStage = stage;
            session.accounts.failure = failures[kind];
            if (kind < 2) {
                EXPECT_FALSE(session.netMarble());
                EXPECT_EQ(session.replies, (std::vector<BYTE>{0}));
            } else {
                try {
                    (void)session.netMarble();
                    FAIL() << "operation failure did not propagate";
                } catch (...) {
                    EXPECT_EQ(std::current_exception(), failures[kind]);
                }
                EXPECT_TRUE(session.replies.empty());
            }
            EXPECT_FALSE(session.player.isFreePass());
            EXPECT_TRUE(session.accounts.updatedPasswords.empty());
            EXPECT_TRUE(session.accounts.netMarbleAccounts.empty());
            expectSessionUnchanged(session);
        }
    }
}

TEST(LoginAuthentication, NonstandardHashFailuresPropagateWithoutBecomingRefusalsOrDiagnostics) {
    for (bool existing : {false, true}) {
        Session session;
        if (existing)
            session.accounts.storedPasswords["Rowan"] = "legacy";
        session.verdict = de::password::Verify::AcceptedRehash;
        session.accounts.failureStage = Stage::Hash;
        session.accounts.failure = std::make_exception_ptr(DatabaseError("hash failed"));
        try {
            (void)session.netMarble();
            FAIL() << "nonstandard hashing failure did not propagate";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), session.accounts.failure);
        }
        EXPECT_TRUE(session.hashFailures.empty());
        EXPECT_TRUE(session.replies.empty());
        EXPECT_FALSE(session.player.isFreePass());
    }
}

struct ExpectedRefusal {
    LoginRejectReason reason;
    BYTE code;
    const char* name;
    int site;
};

const ExpectedRefusal expectedRefusals[] = {{LoginRejectReason::IPBlocked, 16, "IP_DENYED", 1},
                                            {LoginRejectReason::MalformedID, 0, "INVALID_ID_PASSWORD", 2},
                                            {LoginRejectReason::UnknownAccountOrPassword, 0, "INVALID_ID_PASSWORD", 3},
                                            {LoginRejectReason::FreePassAccountMissing, 15, "ETC_ERROR", 4},
                                            {LoginRejectReason::AccessNotAllowed, 15, "ETC_ERROR", 5},
                                            {LoginRejectReason::NotPayAccount, 13, "NOT_PAY_ACCOUNT", 6},
                                            {LoginRejectReason::AlreadyConnected, 1, "ALREADY_CONNECTED", 7},
                                            {LoginRejectReason::AlreadyLoggedOnElsewhere, 1, "ALREADY_CONNECTED", 8},
                                            {LoginRejectReason::NetMarbleAuthorization, 0, "INVALID_ID_PASSWORD", 9},
                                            {LoginRejectReason::WebLoginKeyMismatch, 0, "INVALID_ID_PASSWORD", 10},
                                            {LoginRejectReason::WebLoginKeyNotFound, 21, "NOT_FOUND_KEY", 11},
                                            {LoginRejectReason::WebLoginKeyExpired, 20, "KEY_EXPIRED", 12}};

TEST(LoginAuthentication, EveryRefusalKeepsItsLiteralWireByteAndNumberedDiagnosticAfterSending) {
    for (const auto& expected : expectedRefusals) {
        Session session;
        session.actions.sendRefusal = [&](LoginPlayer& player, LCLoginError& reply) {
            EXPECT_TRUE(session.refusals.empty());
            de::defaultLoginAuthenticationActions().sendRefusal(player, reply);
        };
        de::sendLoginRefusal(session.player, "Rowan", expected.reason, session.actions);
        EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), std::string(1, expected.code));
        EXPECT_EQ(session.refusals,
                  (std::vector<std::tuple<std::string, std::string, int>>{{"Rowan", expected.name, expected.site}}));
        expectSessionUnchanged(session);
    }
}

TEST(LoginAuthentication, RefusalSendingKeepsExceptionIdentityAndDoesNotReportOrSendAgain) {
    const std::exception_ptr failures[] = {
        std::make_exception_ptr(Error("send failed")), std::make_exception_ptr(DatabaseError("send failed")),
        std::make_exception_ptr(std::runtime_error("send failed")), std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        Session session;
        session.accounts.failureStage = Stage::Send;
        session.accounts.failure = failure;
        try {
            de::sendLoginRefusal(session.player, "Rowan", LoginRejectReason::MalformedID, session.actions);
            FAIL() << "send failure did not propagate";
        } catch (...) {
            EXPECT_EQ(std::current_exception(), failure);
        }
        EXPECT_EQ(session.accounts.calls, (std::vector<Stage>{Stage::Send}));
        EXPECT_TRUE(session.refusals.empty());
        expectSessionUnchanged(session);
    }
}

TEST(LoginAuthentication, PacketMutationCannotChangeVerificationHashingWritesOrRefusalIdentity) {
    for (bool existing : {false, true}) {
        Session session;
        if (existing)
            session.accounts.storedPasswords["Rowan"] = "legacy";
        session.verdict = de::password::Verify::AcceptedRehash;
        session.accounts.before = [&](Stage stage) {
            if (stage == Stage::Stored) {
                session.packet.setID("Willow");
                session.packet.setPassword("changed-credential");
            }
        };
        EXPECT_TRUE(session.netMarble());
        EXPECT_EQ(session.accounts.queries, (std::vector<std::string>{"Rowan"}));
        EXPECT_EQ(session.hashes, (std::vector<std::string>{"test-credential"}));
        if (existing) {
            EXPECT_EQ(session.verifications.front().second, "test-credential");
            EXPECT_EQ(session.accounts.updatedPasswords.front().second, "Rowan");
        } else {
            EXPECT_EQ(session.accounts.netMarbleAccounts.front().first, "Rowan");
        }
    }
    Session session;
    session.accounts.storedPasswords["Rowan"] = "stored";
    session.verdict = de::password::Verify::Rejected;
    session.accounts.before = [&](Stage stage) {
        if (stage == Stage::Verify)
            session.packet.setID("Willow");
    };
    EXPECT_FALSE(session.netMarble());
    ASSERT_EQ(session.refusals.size(), 1u);
    EXPECT_EQ(std::get<0>(session.refusals.front()), "Rowan");
}

TEST(LoginAuthentication, RefusalReportingOwnsTheAccountBeforeTheSenderCanChangeItsSource) {
    Session session;
    std::string account = "Rowan";
    session.actions.sendRefusal = [&](LoginPlayer&, LCLoginError&) { account = "Willow"; };
    de::sendLoginRefusal(session.player, account, LoginRejectReason::MalformedID, session.actions);
    ASSERT_EQ(session.refusals.size(), 1u);
    EXPECT_EQ(std::get<0>(session.refusals.front()), "Rowan");
}

TEST(LoginAuthentication, WebAcceptanceConsumesTheOwnedKeyBeforeGrantingTheFreePass) {
    Session session;
    session.validKey();
    session.accounts.before = [&](Stage stage) {
        EXPECT_FALSE(session.player.isFreePass());
        if (stage == Stage::Web) {
            session.packet.setID("Willow");
            session.packet.setPassword("changed-key");
        }
    };
    session.accounts.after = [&](Stage stage) {
        EXPECT_EQ(stage, Stage::Delete);
        EXPECT_FALSE(session.player.isFreePass());
    };
    EXPECT_TRUE(session.web());
    EXPECT_EQ(session.accounts.calls, (std::vector<Stage>{Stage::Web, Stage::Delete}));
    EXPECT_EQ(session.accounts.deletedWebLoginKeys, (std::vector<std::string>{"Rowan"}));
    EXPECT_TRUE(session.accounts.webLoginKeys.empty());
    EXPECT_TRUE(session.player.isFreePass());
    expectSessionUnchanged(session);
}

TEST(LoginAuthentication, WebRefusalsPreserveMissingMismatchAndDatabaseClockPolicy) {
    for (int mode = 0; mode < 5; ++mode) {
        Session session;
        session.validKey();
        if (mode == 0)
            session.accounts.webLoginKeys.clear();
        if (mode == 1) {
            session.packet.setPassword("different");
            session.accounts.webLoginKeys.at("Rowan").now = "2026-09-01 14:00:00";
        }
        if (mode == 2)
            session.accounts.webLoginKeys.at("Rowan").now = "2026-09-01 12:05:01";
        if (mode == 4)
            session.accounts.webLoginKeys.at("Rowan").now = "2026-09-01 11:59:59";
        EXPECT_EQ(session.web(), mode >= 3);
        EXPECT_EQ(session.player.isFreePass(), mode >= 3);
        EXPECT_EQ(session.accounts.deletedWebLoginKeys.size(), mode >= 3 ? 1u : 0u);
        EXPECT_EQ(session.mismatches.size(), mode == 1 ? 1u : 0u);
        if (mode < 3) {
            const BYTE codes[] = {21, 0, 20};
            const int sites[] = {11, 10, 12};
            EXPECT_EQ(session.replies, (std::vector<BYTE>{codes[mode]}));
            ASSERT_EQ(session.refusals.size(), 1u);
            EXPECT_EQ(std::get<2>(session.refusals.front()), sites[mode]);
        }
        expectSessionUnchanged(session);
    }
}

TEST(LoginAuthentication, WebOperationFailuresKeepTheirBoundaryAndDoNotChangeAnExistingFreePass) {
    const std::exception_ptr failures[] = {
        std::make_exception_ptr(Error("operation failed")), std::make_exception_ptr(DatabaseError("operation failed")),
        std::make_exception_ptr(std::runtime_error("operation failed")), std::make_exception_ptr(std::bad_alloc())};
    for (bool previousFreePass : {false, true}) {
        for (const auto stage : {Stage::Web, Stage::Delete, Stage::Send}) {
            for (std::size_t kind = 0; kind < std::size(failures); ++kind) {
                Session session;
                session.player.setFreePass(previousFreePass);
                if (stage != Stage::Send)
                    session.validKey();
                session.accounts.failureStage = stage;
                session.accounts.failure = failures[kind];
                if (kind == 0) {
                    EXPECT_FALSE(session.web());
                } else {
                    try {
                        (void)session.web();
                        FAIL() << "operation failure did not propagate";
                    } catch (...) {
                        EXPECT_EQ(std::current_exception(), failures[kind]);
                    }
                }
                EXPECT_EQ(session.player.isFreePass(), previousFreePass);
                EXPECT_TRUE(session.accounts.deletedWebLoginKeys.empty());
                EXPECT_TRUE(session.refusals.empty());
                expectSessionUnchanged(session);
            }
        }
    }
}

TEST(LoginAuthentication, FailedWriteAcknowledgementsRetainEffectsWithoutGrantingAFreePass) {
    for (const auto stage : {Stage::Update, Stage::Insert, Stage::Delete}) {
        for (bool databaseFailure : {false, true}) {
            Session session;
            if (stage == Stage::Update) {
                session.accounts.storedPasswords["Rowan"] = "legacy";
                session.verdict = de::password::Verify::AcceptedRehash;
            }
            if (stage == Stage::Delete)
                session.validKey();
            const auto failure = databaseFailure ? std::make_exception_ptr(DatabaseError("acknowledgement failed"))
                                                 : std::make_exception_ptr(Error("acknowledgement failed"));
            session.accounts.after = [&](Stage) { std::rethrow_exception(failure); };
            auto authenticate = [&] { return stage == Stage::Delete ? session.web() : session.netMarble(); };
            if (databaseFailure) {
                try {
                    (void)authenticate();
                    FAIL() << "database failure did not propagate";
                } catch (...) {
                    EXPECT_EQ(std::current_exception(), failure);
                }
            } else {
                EXPECT_FALSE(authenticate());
            }
            EXPECT_FALSE(session.player.isFreePass());
            EXPECT_EQ(session.accounts.updatedPasswords.size(), stage == Stage::Update ? 1u : 0u);
            EXPECT_EQ(session.accounts.netMarbleAccounts.size(), stage == Stage::Insert ? 1u : 0u);
            EXPECT_EQ(session.accounts.deletedWebLoginKeys.size(), stage == Stage::Delete ? 1u : 0u);
            session.accounts.after = {};
            session.verdict = de::password::Verify::Accepted;
            EXPECT_EQ(authenticate(), stage != Stage::Delete);
            EXPECT_EQ(session.player.isFreePass(), stage != Stage::Delete);
            EXPECT_EQ(session.accounts.netMarbleAccounts.size(), stage == Stage::Insert ? 1u : 0u);
            EXPECT_EQ(session.accounts.deletedWebLoginKeys.size(), stage == Stage::Delete ? 1u : 0u);
            expectSessionUnchanged(session);
        }
    }
}

TEST(LoginAuthentication, FailureAfterBufferingRetainsTheRefusalWithoutDiagnosticOrAuthentication) {
    Session session;
    const auto failure = std::make_exception_ptr(std::runtime_error("send failed after buffering"));
    session.actions.sendRefusal = [&](LoginPlayer& player, LCLoginError& reply) {
        de::defaultLoginAuthenticationActions().sendRefusal(player, reply);
        std::rethrow_exception(failure);
    };
    try {
        (void)session.web();
        FAIL() << "partial send did not propagate";
    } catch (...) {
        EXPECT_EQ(std::current_exception(), failure);
    }
    EXPECT_EQ(session.player.bufferedBytes().substr(szPacketHeader), std::string(1, 21));
    EXPECT_FALSE(session.player.isFreePass());
    EXPECT_TRUE(session.refusals.empty());
    expectSessionUnchanged(session);
}

TEST(LoginAuthentication, EveryDiagnosticFailureIsContainedAndDiagnosticsCanBeOmitted) {
    const std::exception_ptr failures[] = {nullptr, std::make_exception_ptr(Error("diagnostic failed")),
                                           std::make_exception_ptr(DatabaseError("diagnostic failed")),
                                           std::make_exception_ptr(std::runtime_error("diagnostic failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (int mode = 0; mode < 5; ++mode) {
        for (const auto& failure : failures) {
            Session session;
            const Stage stages[] = {Stage::RefusalReport, Stage::HashReport, Stage::HashReport, Stage::NewReport,
                                    Stage::MismatchReport};
            session.accounts.failureStage = stages[mode];
            session.accounts.failure = failure;
            if (!failure) {
                session.actions.refusal = {};
                session.actions.hashingFailure = {};
                session.actions.newNetMarbleAccount = {};
                session.actions.webKey.mismatch = {};
            }
            if (mode == 1 || mode == 2) {
                if (mode == 2)
                    session.accounts.storedPasswords["Rowan"] = "legacy";
                session.verdict = de::password::Verify::AcceptedRehash;
                session.actions.hashPassword = [](std::string_view) -> std::string {
                    throw std::runtime_error("hash failed");
                };
            }
            if (mode == 4) {
                session.validKey();
                session.packet.setPassword("different");
            }
            if (mode == 0 || mode == 4)
                EXPECT_FALSE(session.web());
            else
                EXPECT_EQ(session.netMarble(), mode != 1);
            EXPECT_EQ(session.player.isFreePass(), mode == 2 || mode == 3);
            EXPECT_EQ(session.replies.size(), mode == 2 || mode == 3 ? 0u : 1u);
            expectSessionUnchanged(session);
        }
    }
}

TEST(LoginAuthentication, AuthenticationAndRefusalRetainAnExistingAccountCleanupOwner) {
    for (int mode = 0; mode < 4; ++mode) {
        Session session;
        ASSERT_TRUE(session.player.loginAccountOwnership().acquire("previous-owner", [] { return true; }));
        const auto* owner = session.player.loginAccountOwnership().account();
        if (mode == 0)
            session.validKey();
        if (mode == 3) {
            session.accounts.storedPasswords["Rowan"] = "stored";
            session.verdict = de::password::Verify::Rejected;
        }
        EXPECT_EQ(mode < 2 ? session.web() : session.netMarble(), mode == 0 || mode == 2);
        EXPECT_EQ(session.player.loginAccountOwnership().account(), owner);
        EXPECT_TRUE(session.player.loginAccountOwnership().owns("previous-owner"));
        EXPECT_EQ(session.player.getID(), "visitor");
        EXPECT_TRUE(session.accounts.markedLoggedOn.empty());
    }
}

int checkPublication(int mode) {
    Session session;
    std::optional<AllocationProbe> probe;
    if (mode == 0)
        session.validKey();
    if (mode == 2 || mode == 3) {
        session.accounts.storedPasswords["Rowan"] = "stored";
        session.verdict = de::password::Verify::AcceptedRehash;
    }
    if (mode == 3) {
        session.actions.verifyPassword = [&](std::string_view, std::string_view) {
            probe.emplace(1);
            return de::password::Verify::Accepted;
        };
    } else {
        session.accounts.after = [&](Stage) { probe.emplace(1); };
    }
    const bool accepted = mode == 0 ? session.web() : session.netMarble();
    if (!probe)
        return 1;
    probe->stopFailing();
    return accepted && session.player.isFreePass() && probe->attempts() == 0 ? 0 : 2;
}

TEST(LoginAuthentication, FreePassPublicationDoesNotAllocateAfterVerificationOrTheFinalWrite) {
    for (int mode = 0; mode < 4; ++mode)
        ASSERT_EXIT(std::_Exit(checkPublication(mode)), ::testing::ExitedWithCode(0), "");
}

class AuthenticationLogs {
public:
    AuthenticationLogs() : previous(std::cout.rdbuf(output.rdbuf())) {
        if (!mkdtemp(directory) || chdir(directory) != 0)
            std::_Exit(90);
    }
    ~AuthenticationLogs() {
        std::cout.rdbuf(previous);
        unlink("loginfail.txt");
        unlink("keydiff.txt");
        chdir("/");
        rmdir(directory);
    }
    std::string console() const {
        return output.str();
    }

private:
    char directory[64] = "/tmp/darkeden-authentication-XXXXXX";
    std::ostringstream output;
    std::streambuf* previous;
};

int checkProductionPasswords() {
    AuthenticationLogs logs;
    for (bool existing : {false, true}) {
        Session session;
        if (existing)
            session.accounts.storedPasswords["Rowan"] = "test-credential";
        session.actions = de::defaultLoginAuthenticationActions();
        if (!session.netMarble() || !session.player.isFreePass())
            return 1;
        const auto& stored = session.accounts.storedPasswords.at("Rowan");
        if (!de::password::isHashed(stored) ||
            de::password::verify(stored, "test-credential") != de::password::Verify::Accepted ||
            session.accounts.updatedPasswords.size() != (existing ? 1u : 0u) ||
            session.accounts.netMarbleAccounts.size() != (existing ? 0u : 1u))
            return 2;
        session.player.setFreePass(false);
        if (!session.netMarble() || session.accounts.updatedPasswords.size() != (existing ? 1u : 0u))
            return 3;
        session.player.setFreePass(false);
        session.packet.setPassword("wrong-credential");
        if (session.netMarble() || session.player.isFreePass() ||
            session.player.bufferedBytes().substr(szPacketHeader) != std::string(1, 0))
            return 4;
    }
    return logs.console().find("NetMarble New Player: Rowan\n") != std::string::npos ? 0 : 5;
}

TEST(LoginAuthentication, ProductionAdaptersVerifyMigrateAndCreateWithArgon2WithoutServerStartup) {
    ASSERT_EXIT(std::_Exit(checkProductionPasswords()), ::testing::ExitedWithCode(0), "");
}

int checkProductionLogs() {
    AuthenticationLogs logs;
    Session session;
    const auto& production = de::defaultLoginAuthenticationActions();
    session.actions.refusal = production.refusal;
    for (const auto& expected : expectedRefusals)
        de::sendLoginRefusal(session.player, "Rowan", expected.reason, session.actions);
    std::ifstream refusals("loginfail.txt");
    std::string line;
    for (const auto& expected : expectedRefusals) {
        const auto suffix = " : Error Code: " + std::string(expected.name) + ", " + std::to_string(expected.site) +
                            ", PlayerID : Rowan";
        if (!std::getline(refusals, line) || !line.ends_with(suffix))
            return 1;
    }
    if (std::getline(refusals, line))
        return 2;
    session.validKey();
    session.accounts.webLoginKeys.at("Rowan").key = "private-database-key";
    session.packet.setPassword("private-packet-key");
    session.actions.webKey = production.webKey;
    if (session.web())
        return 3;
    std::ifstream mismatch("keydiff.txt");
    if (!std::getline(mismatch, line) || !line.ends_with(" : Web login key mismatch, Player ID: Rowan") ||
        line.find("private-database-key") != std::string::npos ||
        line.find("private-packet-key") != std::string::npos || std::getline(mismatch, line) || !logs.console().empty())
        return 4;
    return 0;
}

TEST(LoginAuthentication, ProductionDiagnosticsPreserveRefusalSitesAndOmitBothWebKeys) {
    ASSERT_EXIT(std::_Exit(checkProductionLogs()), ::testing::ExitedWithCode(0), "");
}

int checkDiagnosticAllocation(std::size_t position, int mode) {
    AuthenticationLogs logs;
    Session session;
    const auto& production = de::defaultLoginAuthenticationActions();
    bool invoked = false;
    auto run = [&](auto&& callback) {
        invoked = true;
        AllocationProbe probe(position);
        callback();
    };
    session.actions.refusal = [&](const std::string& account, const char* name, int site) {
        run([&] { production.refusal(account, name, site); });
    };
    if (mode == 1 || mode == 2) {
        if (mode == 2)
            session.accounts.storedPasswords["Rowan"] = "legacy";
        session.verdict = de::password::Verify::AcceptedRehash;
        session.actions.hashPassword = [](std::string_view) -> std::string { throw std::runtime_error("hash failed"); };
        session.actions.hashingFailure = [&](const std::string& account, const char* detail, bool rehash) {
            run([&] { production.hashingFailure(account, detail, rehash); });
        };
        session.actions.refusal = {};
    }
    if (mode == 3) {
        session.validKey();
        session.packet.setPassword("different");
        session.actions.webKey.mismatch = [&](const std::string& account) {
            run([&] { production.webKey.mismatch(account); });
        };
        session.actions.refusal = {};
    }
    if (mode == 4) {
        session.actions.newNetMarbleAccount = [&](const std::string& account) {
            run([&] { production.newNetMarbleAccount(account); });
        };
    }
    const bool accepted = mode == 0 || mode == 3 ? session.web() : session.netMarble();
    if (!invoked || accepted != (mode == 2 || mode == 4) || session.player.isFreePass() != accepted ||
        session.replies.size() != (accepted ? 0u : 1u))
        return 1;
    if (position == 0 && (mode == 1 || mode == 2)) {
        std::ifstream log("loginfail.txt");
        std::string line;
        const auto expected = mode == 1 ? " : Password hashing failed, PlayerID : Rowan : hash failed"
                                        : " : Password rehash failed, PlayerID : Rowan : hash failed";
        if (!std::getline(log, line) || !line.ends_with(expected) || std::getline(log, line))
            return 2;
    }
    return 0;
}

TEST(LoginAuthentication, ProductionDiagnosticAllocationFailuresCannotChangeAuthenticationOrRefusal) {
    for (int mode = 0; mode < 5; ++mode) {
        for (std::size_t position = 0; position <= 16; ++position) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << position);
            ASSERT_EXIT(std::_Exit(checkDiagnosticAllocation(position, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

int checkAllocationFailure(std::size_t position, int mode) {
    auto session = std::make_unique<Session>();
    const std::string account(30, 'a');
    const std::string credential(30, 'p');
    session->packet.setID(account);
    session->packet.setPassword(credential);
    if (mode == 1 || mode == 2 || mode == 7)
        session->accounts.storedPasswords[account] = std::string(50, 's');
    session->verdict = mode == 2 ? de::password::Verify::Rejected : de::password::Verify::AcceptedRehash;
    if (mode == 3 || mode == 4 || mode == 6)
        session->accounts.webLoginKeys[account] = {mode == 4 ? "different" : credential, "2026-09-01 12:00:00",
                                                   "2026-09-01 12:00:30"};
    const auto failure = std::make_exception_ptr(DatabaseError("delete failed"));
    if (mode == 6) {
        session->accounts.failureStage = Stage::Delete;
        session->accounts.failure = failure;
    }
    const auto hashFailure = std::make_exception_ptr(std::runtime_error("hash failed"));
    if (mode == 7) {
        session->actions.hashPassword = [&](std::string_view) -> std::string { std::rethrow_exception(hashFailure); };
        session->actions.hashingFailure = [&](const std::string&, const char*, bool) {
            std::rethrow_exception(failure);
        };
    }
    auto authenticate = [&] { return mode >= 3 && mode <= 6 ? session->web() : session->netMarble(); };
    AllocationProbe probe(position);
    bool accepted = false;
    bool allocationFailed = false;
    bool databaseFailed = false;
    try {
        accepted = authenticate();
    } catch (const std::bad_alloc&) {
        allocationFailed = true;
    } catch (...) {
        databaseFailed = mode == 6 && std::current_exception() == failure;
        if (!databaseFailed)
            return 1;
    }
    probe.stopFailing();
    if ((allocationFailed && !probe.rejected()) || (position == 96 && probe.rejected()) ||
        (!probe.rejected() &&
         (accepted != (mode == 0 || mode == 1 || mode == 3 || mode == 7) || databaseFailed != (mode == 6))))
        return 2;
    if (session->player.isFreePass() != accepted || session->player.getID() != "visitor" ||
        session->player.getPlayerStatus() != LPS_BEGIN_SESSION || session->player.loginAccountOwnership().account())
        return 3;
    if (!session->accounts.deletedWebLoginKeys.empty() && !accepted)
        return 4;
    if (!accepted && mode != 2 && mode != 4 && mode != 5) {
        session->accounts.failure = nullptr;
        session->verdict = de::password::Verify::Accepted;
        if (!authenticate())
            return 5;
    }
    session.reset();
    return probe.outstanding() == 0 ? 0 : 6;
}

TEST(LoginAuthentication, AllocationFailuresPreserveAuthenticationStateAndCleanupThroughRetry) {
    for (int mode = 0; mode < 8; ++mode) {
        for (std::size_t position = 1; position <= 96; ++position) {
            SCOPED_TRACE(::testing::Message() << mode << "/" << position);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(position, mode)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
