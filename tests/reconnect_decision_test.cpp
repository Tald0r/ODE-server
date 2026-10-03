// The loginserver's reconnect decision
// (src/server/loginserver/ReconnectDecision.cpp): every refusal with the
// input that triggers it, the precedence between them, and what the
// decision writes to the session on the way. The repository and the
// session are fakes, so neither a database nor a socket is involved; the
// MySQL-backed integration tier
// (tests/integration/mysql_loginserver_repository_test.cpp) is the
// authority on what the real repository answers.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "FakeLoginAccountRepository.h"
#include "ReconnectDecision.h"

namespace {

// The session state the decision sets, recorded rather than applied.
class FakeReconnectSession : public ReconnectSession {
public:
    de::LoginAccountOwnership ownership;
    de::LoginAccountOwnership& accountOwnership() noexcept override {
        return ownership;
    }
    static constexpr int kUnset = -1;

    int worldID = kUnset;
    int serverGroupID = kUnset;
    std::vector<std::string> ids;

    struct PayPlayValue {
        int payType;
        std::string payPlayDate;
        int payPlayHours;
        unsigned int payPlayFlag;
    };
    std::vector<PayPlayValue> payPlayValues;

    void setWorldID(int value) override {
        worldID = value;
    }

    void setServerGroupID(int value) override {
        serverGroupID = value;
    }

    void setID(const std::string& playerID) override {
        ids.push_back(playerID);
    }

    void setPayPlayValue(int payType, const std::string& payPlayDate, int payPlayHours,
                         unsigned int payPlayFlag) override {
        PayPlayValue value;
        value.payType = payType;
        value.payPlayDate = payPlayDate;
        value.payPlayHours = payPlayHours;
        value.payPlayFlag = payPlayFlag;
        payPlayValues.push_back(value);
    }

    // Only a build with the pay system applied at login reaches this, so
    // nothing here calls it.
    bool loginPayPlay(int, const std::string&, int, unsigned int, const std::string&, const std::string&) override {
        ADD_FAILURE() << "loginPayPlay is not compiled into this build's decision";
        return true;
    }
};

const char* const kPlayerID = "reconnecting";

ReconnectRequest request() {
    ReconnectRequest req;
    req.playerID = kPlayerID;
    req.connectIP = "10.0.0.1";
    req.loginServerID = 12;
    return req;
}

// The account the request above names, in whatever state the caller wants.
void addAccount(FakeLoginAccountRepository& repository, const LoginReconnectRow& row) {
    repository.reconnectAccounts[kPlayerID] = row;
}

// --- refusals -------------------------------------------------------------

TEST(DecideReconnectLogin, AnAccountWithNoRowIsRefusedBeforeAnySessionWrite) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(ReconnectRejectReason::NoSuchAccount, outcome.rejection().reason);
    EXPECT_EQ(FakeReconnectSession::kUnset, session.worldID);
    EXPECT_EQ(FakeReconnectSession::kUnset, session.serverGroupID);
    EXPECT_EQ(0, repository.markLoggedOnForReconnectCalls);
}

TEST(DecideReconnectLogin, AnAccountInAGameIsRefusedAndItsIdCleared) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    row.logOn = "GAME";
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(ReconnectRejectReason::AlreadyInGame, outcome.rejection().reason);
    // The message the handler builds names the column that refused.
    EXPECT_EQ("GAME", outcome.rejection().logOn);
    ASSERT_EQ(1u, session.ids.size());
    EXPECT_EQ("NONE", session.ids[0]);
    // Nothing was marked logged on for an account that is in a game.
    EXPECT_EQ(0, repository.markLoggedOnForReconnectCalls);
}

TEST(DecideReconnectLogin, AnAccountAlreadyLoggedOnIsRefusedTheSameWay) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    row.logOn = "LOGON";
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(ReconnectRejectReason::AlreadyInGame, outcome.rejection().reason);
    EXPECT_EQ("LOGON", outcome.rejection().logOn);
}

TEST(DecideReconnectLogin, TheAccountsLocationIsKeptEvenByARefusalAfterTheRead) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    row.currentWorldID = 5;
    row.currentServerGroupID = 8;
    row.logOn = "GAME";
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(5, session.worldID);
    EXPECT_EQ(8, session.serverGroupID);
}

TEST(DecideReconnectLogin, ARowThatAnotherSessionTookFirstIsRefused) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    addAccount(repository, FakeLoginAccountRepository::allowedReconnectAccount());
    repository.markLoggedOnForReconnectSucceeds = false;

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(ReconnectRejectReason::AlreadyLoggedOnElsewhere, outcome.rejection().reason);
    EXPECT_EQ(session.ownership.account(), nullptr);
    EXPECT_EQ(1, repository.markLoggedOnForReconnectCalls);
    // The account id stands; only an account in a game loses it.
    EXPECT_TRUE(session.ids.empty());
}

TEST(DecideReconnectLogin, AnAccountWhoseAccessIsNotAllowIsRefused) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    row.access = "DENY";
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(ReconnectRejectReason::AccessNotAllowed, outcome.rejection().reason);
}

TEST(DecideReconnectLogin, AnAccessRefusalStillLeavesTheRowMarkedLoggedOn) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    row.access = "DENY";
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isRejected());
    // The compare-and-set runs before the access check, so a refused
    // account retains ownership so disconnect can release it.
    ASSERT_EQ(1u, repository.markedLoggedOnForReconnect.size());
    EXPECT_EQ(12, repository.markedLoggedOnForReconnect[0].first);
    EXPECT_EQ(kPlayerID, repository.markedLoggedOnForReconnect[0].second);
    EXPECT_TRUE(session.ownership.owns(kPlayerID));
}

TEST(DecideReconnectLogin, AnAccessRefusalOnARowThatIsNeitherLogoffNorInGameMarksNothing) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    // The shipped schema declares LogOn as an enum of LOGOFF / LOGON /
    // GAME, so this is only reachable through a hand-edited column.
    row.logOn = "";
    row.access = "DENY";
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(ReconnectRejectReason::AccessNotAllowed, outcome.rejection().reason);
    EXPECT_EQ(0, repository.markLoggedOnForReconnectCalls);
}

// --- an accepted reconnect -------------------------------------------------

TEST(DecideReconnectLogin, ALoggedOffAllowedAccountIsAccepted) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    row.currentWorldID = 2;
    row.currentServerGroupID = 4;
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(2, outcome.events().worldID);
    EXPECT_EQ(4, outcome.events().serverGroupID);
    EXPECT_EQ(2, session.worldID);
    EXPECT_EQ(4, session.serverGroupID);
    EXPECT_TRUE(session.ownership.owns(kPlayerID));
}

TEST(DecideReconnectLogin, AnAcceptedReconnectMarksTheRowLoggedOnForThisLoginServer) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    addAccount(repository, FakeLoginAccountRepository::allowedReconnectAccount());

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isOk());
    ASSERT_EQ(1u, repository.markedLoggedOnForReconnect.size());
    EXPECT_EQ(12, repository.markedLoggedOnForReconnect[0].first);
    EXPECT_EQ(kPlayerID, repository.markedLoggedOnForReconnect[0].second);
}

TEST(DecideReconnectLogin, ThePayPlayColumnsAreHandedToTheSession) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    row.payType = 2;
    row.payPlayDate = "2026-01-02 03:04:05";
    row.payPlayHours = 40;
    row.payPlayFlag = 7;
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isOk());
    ASSERT_EQ(1u, session.payPlayValues.size());
    EXPECT_EQ(2, session.payPlayValues[0].payType);
    EXPECT_EQ("2026-01-02 03:04:05", session.payPlayValues[0].payPlayDate);
    EXPECT_EQ(40, session.payPlayValues[0].payPlayHours);
    EXPECT_EQ(7u, session.payPlayValues[0].payPlayFlag);
}

TEST(DecideReconnectLogin, ARowThatIsNeitherLogoffNorInGameIsAcceptedWithoutTheMark) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    LoginReconnectRow row = FakeLoginAccountRepository::allowedReconnectAccount();
    row.logOn = "";
    addAccount(repository, row);

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(0, repository.markLoggedOnForReconnectCalls);
}

TEST(DecideReconnectLogin, TheAccountRowIsReadExactlyOnce) {
    FakeLoginAccountRepository repository;
    FakeReconnectSession session;
    addAccount(repository, FakeLoginAccountRepository::allowedReconnectAccount());

    Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request(), repository, session);

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(1, repository.loadAccountForReconnectCalls);
}

} // namespace
