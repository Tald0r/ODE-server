// The loginserver's login decision (src/server/loginserver/LoginDecision.cpp):
// every refusal with the input that triggers it, the precedence between
// them, and what an accepted login carries. The repository is a fake, so no
// database is involved; the MySQL-backed integration tier
// (tests/integration/mysql_loginserver_repository_test.cpp) is the authority
// on what the real repository answers.
//
// Password checking runs against the real argon2id implementation, so the
// hashes here are computed at test time or taken from upstream's own
// vectors (tests/password_hash_test.cpp pins those).

#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "Exception.h"
#include "FakeLoginAccountRepository.h"
#include "LoginDecision.h"
#include "PasswordHash.h"

namespace {

// One of upstream argon2's own vectors: argon2id over "password" at two
// passes, so it verifies but asks to be rewritten under this server's
// three-pass parameters.
const char* const kUpstreamTwoPassHash =
    "$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$CTFhFdXPJO1aFaMaO6Mm5c8y7cJHAph8ArZWb2GRPPc";

VSDateTime at(int year, int month, int day, int hour = 12, int minute = 0, int second = 0) {
    return VSDateTime(VSDate(year, month, day), VSTime(hour, minute, second));
}

// LoginSession that records what the decision set and answers the deadlines
// the test gave it. The real implementation parses those out of the two
// date columns; a test states them outright so the boundaries are exact.
class FakeLoginSession : public LoginSession {
public:
    de::LoginAccountOwnership ownership;
    de::LoginAccountOwnership& accountOwnership() noexcept override {
        return ownership;
    }
    int serverGroupID = -1;
    int setServerGroupIDCalls = 0;

    bool payPlayValueSet = false;
    int payType = 0;
    std::string payPlayDate;
    int payPlayHours = 0;
    unsigned int payPlayFlag = 0;
    std::string familyPayPlayDate;

    VSDateTime payPlayAvailable;
    VSDateTime familyPayPlayAvailable;

    void setServerGroupID(int id) override {
        setServerGroupIDCalls++;
        serverGroupID = id;
    }

    void setPayPlayValue(int type, const std::string& date, int hours, unsigned int flag,
                         const std::string& familyDate) override {
        payPlayValueSet = true;
        payType = type;
        payPlayDate = date;
        payPlayHours = hours;
        payPlayFlag = flag;
        familyPayPlayDate = familyDate;
    }

    const VSDateTime& payPlayAvailableDateTime() const override {
        return payPlayAvailable;
    }

    const VSDateTime& familyPayPlayAvailableDateTime() const override {
        return familyPayPlayAvailable;
    }

    bool loginPayPlay(int, const std::string&, int, unsigned int, const std::string&, const std::string&) override {
        // Only a build with the pay system applied at login calls this.
        return true;
    }
};

// A request that a repository holding allowedAccount("rowan") accepts.
LoginRequest passwordRequest(const std::string& playerID = "rowan") {
    LoginRequest request;
    request.playerID = playerID;
    request.connectIP = "10.0.0.1";
    request.webLogin = false;
    request.freePass = false;
    request.passwordAccepted = true;
    request.failureCount = 0;
    request.loginServerID = 7;
    return request;
}

// --- checkStoredPassword --------------------------------------------------

TEST(CheckStoredPassword, AnUnknownAccountIsRefusedWithoutARehash) {
    FakeLoginAccountRepository repository;

    const PasswordCheck check = checkStoredPassword("nobody", "secret", repository);

    EXPECT_FALSE(check.accepted);
    EXPECT_FALSE(check.rehash);
    EXPECT_EQ(1, repository.loadPasswordHashCalls);
}

TEST(CheckStoredPassword, ACurrentHashAcceptsItsOwnPasswordAndAsksForNoRewrite) {
    FakeLoginAccountRepository repository;
    repository.storedPasswords["rowan"] = de::password::hash("correct horse");

    const PasswordCheck check = checkStoredPassword("rowan", "correct horse", repository);

    EXPECT_TRUE(check.accepted);
    EXPECT_FALSE(check.rehash);
    EXPECT_TRUE(check.hash.empty());
}

TEST(CheckStoredPassword, AWrongPasswordIsRefused) {
    FakeLoginAccountRepository repository;
    repository.storedPasswords["rowan"] = de::password::hash("correct horse");

    const PasswordCheck check = checkStoredPassword("rowan", "wrong horse", repository);

    EXPECT_FALSE(check.accepted);
    EXPECT_FALSE(check.rehash);
}

TEST(CheckStoredPassword, ALegacyPlaintextRowIsAcceptedAndRewrittenAsAHash) {
    FakeLoginAccountRepository repository;
    repository.storedPasswords["rowan"] = "111111";

    const PasswordCheck check = checkStoredPassword("rowan", "111111", repository);

    ASSERT_TRUE(check.accepted);
    ASSERT_TRUE(check.rehash);
    EXPECT_NE("111111", check.hash);
    EXPECT_TRUE(de::password::isHashed(check.hash));
    EXPECT_EQ(de::password::Verify::Accepted, de::password::verify(check.hash, "111111"));

    // The decision does not write; the handler does.
    EXPECT_TRUE(repository.updatedPasswords.empty());
}

TEST(CheckStoredPassword, AHashUnderOtherParametersIsAcceptedAndRewritten) {
    FakeLoginAccountRepository repository;
    repository.storedPasswords["rowan"] = kUpstreamTwoPassHash;

    const PasswordCheck check = checkStoredPassword("rowan", "password", repository);

    ASSERT_TRUE(check.accepted);
    ASSERT_TRUE(check.rehash);
    EXPECT_EQ(de::password::Verify::Accepted, de::password::verify(check.hash, "password"));
}

TEST(CheckStoredPassword, AWrongPasswordAgainstAPlaintextRowAsksForNoRewrite) {
    FakeLoginAccountRepository repository;
    repository.storedPasswords["rowan"] = "111111";

    const PasswordCheck check = checkStoredPassword("rowan", "111112", repository);

    EXPECT_FALSE(check.accepted);
    EXPECT_FALSE(check.rehash);
}

// --- isBlockedIP ----------------------------------------------------------

LoginIPBlockRow block(int ipClass, int first, int last) {
    LoginIPBlockRow row;
    row.ipClass = ipClass;
    row.first = first;
    row.last = last;
    return row;
}

TEST(IsBlockedIP, AnAddressWithNoMatchingEntryIsAllowed) {
    FakeLoginAccountRepository repository;

    EXPECT_FALSE(isBlockedIP("61.78.53.228", repository));
    EXPECT_EQ(1, repository.loadIPBlocksCalls);
}

TEST(IsBlockedIP, ClassZeroBoundsTheLastOctet) {
    FakeLoginAccountRepository repository;
    repository.ipBlocks.push_back(block(0, 100, 200));

    EXPECT_FALSE(isBlockedIP("61.78.53.99", repository));
    EXPECT_TRUE(isBlockedIP("61.78.53.100", repository));
    EXPECT_TRUE(isBlockedIP("61.78.53.200", repository));
    EXPECT_FALSE(isBlockedIP("61.78.53.201", repository));
}

TEST(IsBlockedIP, ClassOneBoundsTheSecondOctetAndClassTwoTheThird) {
    FakeLoginAccountRepository classOne;
    classOne.ipBlocks.push_back(block(1, 78, 78));
    EXPECT_TRUE(isBlockedIP("61.78.53.228", classOne));
    EXPECT_FALSE(isBlockedIP("61.79.53.228", classOne));

    FakeLoginAccountRepository classTwo;
    classTwo.ipBlocks.push_back(block(2, 50, 55));
    EXPECT_TRUE(isBlockedIP("61.78.53.228", classTwo));
    EXPECT_FALSE(isBlockedIP("61.78.56.228", classTwo));
}

TEST(IsBlockedIP, AnEntryOfAnUnknownClassBlocksOutright) {
    FakeLoginAccountRepository repository;
    repository.ipBlocks.push_back(block(3, 0, 0));

    EXPECT_TRUE(isBlockedIP("61.78.53.228", repository));
}

TEST(IsBlockedIP, TheFirstMatchingEntryDecides) {
    FakeLoginAccountRepository repository;
    repository.ipBlocks.push_back(block(0, 0, 10));
    repository.ipBlocks.push_back(block(0, 220, 230));

    EXPECT_TRUE(isBlockedIP("61.78.53.228", repository));
}

// --- decideWebLoginKey ----------------------------------------------------

TEST(DecideWebLoginKey, NoRowForTheAccountIsRefusedAsAMissingKey) {
    FakeLoginAccountRepository repository;

    Outcome<void, LoginRejection> outcome = decideWebLoginKey("rowan", "abcd", repository, {});

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::WebLoginKeyNotFound, outcome.rejection().reason);
    EXPECT_FALSE(outcome.rejection().beginSession);
}

TEST(DecideWebLoginKey, AKeyThatDoesNotMatchIsRefusedAsABadPassword) {
    FakeLoginAccountRepository repository;
    FakeLoginAccountRepository::WebLoginKeyRow row;
    row.key = "abcd";
    row.createTime = "2010-06-15 12:00:00";
    row.now = "2010-06-15 12:00:10";
    repository.webLoginKeys["rowan"] = row;

    Outcome<void, LoginRejection> outcome = decideWebLoginKey("rowan", "efgh", repository, {});

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::WebLoginKeyMismatch, outcome.rejection().reason);
}

TEST(DecideWebLoginKey, TheKeyLivesForExactlyFiveMinutes) {
    FakeLoginAccountRepository::WebLoginKeyRow row;
    row.key = "abcd";
    row.createTime = "2010-06-15 12:00:00";

    {
        FakeLoginAccountRepository repository;
        row.now = "2010-06-15 12:05:00"; // 300 seconds
        repository.webLoginKeys["rowan"] = row;

        Outcome<void, LoginRejection> outcome = decideWebLoginKey("rowan", "abcd", repository, {});
        EXPECT_TRUE(outcome.isOk());
    }

    {
        FakeLoginAccountRepository repository;
        row.now = "2010-06-15 12:05:01"; // 301 seconds
        repository.webLoginKeys["rowan"] = row;

        Outcome<void, LoginRejection> outcome = decideWebLoginKey("rowan", "abcd", repository, {});
        ASSERT_TRUE(outcome.isRejected());
        EXPECT_EQ(LoginRejectReason::WebLoginKeyExpired, outcome.rejection().reason);
    }
}

TEST(DecideWebLoginKey, AMismatchIsAnsweredBeforeTheAgeIsLookedAt) {
    FakeLoginAccountRepository repository;
    FakeLoginAccountRepository::WebLoginKeyRow row;
    row.key = "abcd";
    row.createTime = "2010-06-15 12:00:00";
    row.now = "2010-06-15 23:00:00";
    repository.webLoginKeys["rowan"] = row;

    Outcome<void, LoginRejection> outcome = decideWebLoginKey("rowan", "efgh", repository, {});

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::WebLoginKeyMismatch, outcome.rejection().reason);
}

TEST(DecideWebLoginKey, TheDecisionDoesNotDeleteTheKey) {
    FakeLoginAccountRepository repository;
    FakeLoginAccountRepository::WebLoginKeyRow row;
    row.key = "abcd";
    row.createTime = "2010-06-15 12:00:00";
    row.now = "2010-06-15 12:00:10";
    repository.webLoginKeys["rowan"] = row;

    Outcome<void, LoginRejection> outcome = decideWebLoginKey("rowan", "abcd", repository, {});

    EXPECT_TRUE(outcome.isOk());
    EXPECT_TRUE(repository.deletedWebLoginKeys.empty());
}

TEST(DecideWebLoginKey, OnlyAMismatchReportsTheAccountWithoutExposingEitherKey) {
    for (int mode = 0; mode < 4; ++mode) {
        FakeLoginAccountRepository repository;
        if (mode != 0)
            repository.webLoginKeys["rowan"] = {"stored-key", "2026-09-01 12:00:00",
                                                mode == 2 ? "2026-09-01 12:05:01" : "2026-09-01 12:00:30"};
        std::vector<std::string> reported;
        WebLoginKeyActions actions{[&](const std::string& account) { reported.push_back(account); }};
        const auto outcome =
            decideWebLoginKey("rowan", mode == 1 ? "different-key" : "stored-key", repository, actions);
        EXPECT_EQ(outcome.isOk(), mode == 3);
        EXPECT_EQ(reported, mode == 1 ? std::vector<std::string>{"rowan"} : std::vector<std::string>{});
        EXPECT_TRUE(repository.deletedWebLoginKeys.empty());
    }
}

TEST(DecideWebLoginKey, EveryMismatchDiagnosticFailureKeepsTheOriginalRefusal) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(Error("diagnostic failed")),
                                           std::make_exception_ptr(DatabaseError("diagnostic failed")),
                                           std::make_exception_ptr(std::runtime_error("diagnostic failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        FakeLoginAccountRepository repository;
        repository.webLoginKeys["rowan"] = {"stored-key", "2026-09-01 12:00:00", "2026-09-01 12:00:30"};
        WebLoginKeyActions actions{[&](const std::string&) { std::rethrow_exception(failure); }};
        const auto outcome = decideWebLoginKey("rowan", "different-key", repository, actions);
        ASSERT_TRUE(outcome.isRejected());
        EXPECT_EQ(outcome.rejection().reason, LoginRejectReason::WebLoginKeyMismatch);
        EXPECT_TRUE(repository.deletedWebLoginKeys.empty());
    }
}

// --- isAdultByBirthday ----------------------------------------------------

TEST(IsAdultByBirthday, EighteenYearsToTheDayCounts) {
    const VSDateTime now = at(2010, 6, 15);

    // The threshold is the tm_year form of 2010 minus 18, so 920615.
    EXPECT_TRUE(isAdultByBirthday("920614", now));
    EXPECT_TRUE(isAdultByBirthday("920615", now));
    EXPECT_FALSE(isAdultByBirthday("920616", now));
}

TEST(IsAdultByBirthday, TheThresholdMovesWithTheDay) {
    EXPECT_FALSE(isAdultByBirthday("920615", at(2010, 6, 14)));
    EXPECT_TRUE(isAdultByBirthday("920615", at(2010, 6, 16)));
}

TEST(IsAdultByBirthday, AfterTwoThousandEighteenEverySixDigitNumberIsAdult) {
    // The threshold is written in the tm_year form, so from 2018 on it has
    // seven digits and no six-digit birthday can exceed it.
    const VSDateTime now = at(2026, 1, 1);

    EXPECT_TRUE(isAdultByBirthday("991231", now));
    EXPECT_TRUE(isAdultByBirthday("000101", now));

    // The eight-digit Chinese forms do exceed it.
    EXPECT_FALSE(isAdultByBirthday("19900101", now));
}

// --- decideLogin: the refusals --------------------------------------------

TEST(DecideLogin, AnIDWithSQLQuotingIsRefusedBeforeAnyRead) {
    const char* const ids[] = {"row'an", "row\\an"};

    for (unsigned int i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        FakeLoginAccountRepository repository;
        FakeLoginSession session;

        Outcome<LoginAccepted, LoginRejection> outcome =
            decideLogin(passwordRequest(ids[i]), repository, session, at(2010, 6, 15));

        ASSERT_TRUE(outcome.isRejected()) << ids[i];
        EXPECT_EQ(LoginRejectReason::MalformedID, outcome.rejection().reason);
        EXPECT_FALSE(outcome.rejection().beginSession);
        EXPECT_EQ(0, repository.loadAccountCalls);
        EXPECT_EQ(0, session.setServerGroupIDCalls);
    }
}

TEST(DecideLogin, AnUnknownAccountAndAWrongPasswordGetTheSameAnswer) {
    {
        FakeLoginAccountRepository repository;
        FakeLoginSession session;

        Outcome<LoginAccepted, LoginRejection> outcome =
            decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

        ASSERT_TRUE(outcome.isRejected());
        EXPECT_EQ(LoginRejectReason::UnknownAccountOrPassword, outcome.rejection().reason);
    }

    {
        FakeLoginAccountRepository repository;
        FakeLoginSession session;
        repository.accounts["rowan"] = FakeLoginAccountRepository::allowedAccount("rowan");

        LoginRequest request = passwordRequest();
        request.passwordAccepted = false;

        Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

        ASSERT_TRUE(outcome.isRejected());
        EXPECT_EQ(LoginRejectReason::UnknownAccountOrPassword, outcome.rejection().reason);
        EXPECT_TRUE(outcome.rejection().beginSession);
        EXPECT_EQ(0, repository.markLoggedOnCalls);
    }
}

TEST(DecideLogin, TheFailureCounterIsWrittenBackUnchangedUntilItIsPastTheLimit) {
    // The counter is read and stored again as it stands; nothing on this
    // path increments it, so a session only disconnects once something else
    // has raised it past three.
    for (unsigned int count = 0; count <= 3; count++) {
        FakeLoginAccountRepository repository;
        FakeLoginSession session;

        LoginRequest request = passwordRequest();
        request.failureCount = count;

        Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

        ASSERT_TRUE(outcome.isRejected()) << count;
        EXPECT_TRUE(outcome.rejection().touchesFailureCount);
        EXPECT_EQ(count, outcome.rejection().failureCount);
        EXPECT_FALSE(outcome.rejection().disconnect) << count;
    }

    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginRequest request = passwordRequest();
    request.failureCount = 4;

    Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_TRUE(outcome.rejection().disconnect);
    EXPECT_EQ(4u, outcome.rejection().failureCount);
}

TEST(DecideLogin, AFreePassAccountWithNoRowIsRefusedWithTheAccountError) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginRequest request = passwordRequest();
    request.freePass = true;

    Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::FreePassAccountMissing, outcome.rejection().reason);
    EXPECT_TRUE(outcome.rejection().beginSession);
    EXPECT_FALSE(outcome.rejection().touchesFailureCount);
    EXPECT_EQ(1, repository.loadAccountForFreePassCalls);
}

TEST(DecideLogin, AnAccountWhoseAccessIsNotAllowIsRefused) {
    const char* const values[] = {"DENY", "WAIT", "", "allow"};

    for (unsigned int i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        FakeLoginAccountRepository repository;
        FakeLoginSession session;

        LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
        row.access = values[i];
        repository.accounts["rowan"] = row;

        Outcome<LoginAccepted, LoginRejection> outcome =
            decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

        ASSERT_TRUE(outcome.isRejected()) << values[i];
        EXPECT_EQ(LoginRejectReason::AccessNotAllowed, outcome.rejection().reason);
        EXPECT_TRUE(outcome.rejection().beginSession);

        // The session already took the account's server group, and the
        // pay-play columns are not reached.
        EXPECT_EQ(3, session.serverGroupID);
        EXPECT_FALSE(session.payPlayValueSet);
    }
}

TEST(DecideLogin, AnAccountThatReadsLOGONIsRefusedEvenFromItsOwnAddress) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.logOn = "LOGON";
    row.loginIP = "10.0.0.1";
    repository.accounts["rowan"] = row;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::AlreadyConnected, outcome.rejection().reason);
    EXPECT_TRUE(outcome.rejection().touchesFailureCount);
    EXPECT_TRUE(outcome.rejection().beginSession);

    // The pay-play columns are stored before this refusal.
    EXPECT_TRUE(session.payPlayValueSet);
    EXPECT_EQ(0, repository.markLoggedOnCalls);
}

TEST(DecideLogin, AnAccountInAGameIsRefusedFromAnotherAddress) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.logOn = "GAME";
    row.loginIP = "10.0.0.2";
    repository.accounts["rowan"] = row;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::AlreadyConnected, outcome.rejection().reason);
}

TEST(DecideLogin, ARowThatDoesNotFlipToLOGONIsHeldByAnotherSession) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    repository.accounts["rowan"] = FakeLoginAccountRepository::allowedAccount("rowan");
    repository.markLoggedOnSucceeds = false;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::AlreadyLoggedOnElsewhere, outcome.rejection().reason);
    EXPECT_EQ(session.ownership.account(), nullptr);
    EXPECT_TRUE(outcome.rejection().beginSession);
    EXPECT_FALSE(outcome.rejection().touchesFailureCount);
}

// NotPayAccount is the remaining refusal. It comes from the pay-system
// branch, which only a build that applies the pay system at login compiles;
// this build has no billing backend and does not.

// --- decideLogin: precedence ----------------------------------------------

TEST(DecideLogin, AMalformedIDWinsOverEveryOtherReason) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("row'an");
    row.access = "DENY";
    row.logOn = "LOGON";
    repository.accounts["row'an"] = row;

    LoginRequest request = passwordRequest("row'an");
    request.passwordAccepted = false;

    Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::MalformedID, outcome.rejection().reason);
}

TEST(DecideLogin, AWrongPasswordWinsOverARefusedAccount) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.access = "DENY";
    row.logOn = "LOGON";
    repository.accounts["rowan"] = row;

    LoginRequest request = passwordRequest();
    request.passwordAccepted = false;

    Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::UnknownAccountOrPassword, outcome.rejection().reason);
}

TEST(DecideLogin, ARefusedAccountWinsOverBeingConnectedAlready) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.access = "DENY";
    row.logOn = "LOGON";
    repository.accounts["rowan"] = row;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::AccessNotAllowed, outcome.rejection().reason);
}

TEST(DecideLogin, BeingConnectedAlreadyWinsOverTheRowNotFlipping) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.logOn = "LOGON";
    repository.accounts["rowan"] = row;
    repository.markLoggedOnSucceeds = false;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(LoginRejectReason::AlreadyConnected, outcome.rejection().reason);
}

// --- decideLogin: the accepted login --------------------------------------

TEST(DecideLogin, AnOrdinaryLoginIsAcceptedAndFlipsTheRowToLOGON) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.ssn = "920615-1234567";
    repository.accounts["rowan"] = row;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    const LoginAccepted& accepted = outcome.events();

    EXPECT_EQ(LoginNextStep::LoginOK, accepted.next);
    EXPECT_EQ("rowan", accepted.playerID);
    EXPECT_EQ("920615-1234567", accepted.ssn);
    EXPECT_EQ("123-456", accepted.zipCode);
    EXPECT_EQ(3, accepted.currentServerGroupID);
    EXPECT_TRUE(accepted.adult);
    EXPECT_FALSE(accepted.family);
    EXPECT_FALSE(accepted.grantPremiumWeek);

    // A free account has no expiry to report.
    EXPECT_EQ(0xfffe, accepted.lastDays);

    EXPECT_EQ(3, session.serverGroupID);
    EXPECT_TRUE(session.payPlayValueSet);

    ASSERT_EQ(1u, repository.markedLoggedOn.size());
    EXPECT_EQ("10.0.0.1", repository.markedLoggedOn[0].ip);
    EXPECT_EQ(7, repository.markedLoggedOn[0].loginServerID);
    EXPECT_EQ("rowan", repository.markedLoggedOn[0].playerID);
    EXPECT_TRUE(session.ownership.owns("rowan"));
}

TEST(DecideLogin, TheRowSpellingOfTheAccountReplacesTheRequestedOne) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("Rowan");
    repository.accounts["rowan"] = row;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ("Rowan", outcome.events().playerID);
    EXPECT_EQ("Rowan", repository.markedLoggedOn[0].playerID);
    EXPECT_TRUE(session.ownership.owns("Rowan"));
}

TEST(DecideLogin, AnAccountInAGameFromTheSameAddressAsksForACharacterKick) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.logOn = "GAME";
    row.loginIP = "10.0.0.1";
    row.ssn = "920615-1234567";
    repository.accounts["rowan"] = row;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(LoginNextStep::KickCharacter, outcome.events().next);
    EXPECT_TRUE(outcome.events().adult);

    // The row is not flipped; the game server answers first.
    EXPECT_EQ(0, repository.markLoggedOnCalls);
}

TEST(DecideLogin, AnUnknownLogOnValueNeitherRepliesNorRefuses) {
    // The shipped schema declares LogOn as an enum of LOGOFF, LOGON and
    // GAME, so nothing outside those three reaches this.
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.logOn = "";
    repository.accounts["rowan"] = row;

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(LoginNextStep::NoReply, outcome.events().next);
    EXPECT_EQ(0, repository.markLoggedOnCalls);
}

TEST(DecideLogin, AWebLoginReadsItsOwnProjectionAndGetsAPlaceholderZipcode) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.ssn = "920615-1234567";
    repository.accounts["rowan"] = row;

    LoginRequest request = passwordRequest();
    request.webLogin = true;
    // A web login is a free pass as well; the web branch takes precedence.
    request.freePass = true;

    Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ("920615-1234567", outcome.events().ssn);
    EXPECT_EQ("000-000", outcome.events().zipCode);
    EXPECT_TRUE(outcome.events().adult);
    EXPECT_EQ(1, repository.loadAccountForWebLoginCalls);
    EXPECT_EQ(0, repository.loadAccountCalls);
}

TEST(DecideLogin, AFreePassLoginIsNotClassifiedAsAdultAtAll) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.ssn = "920615-1234567";
    repository.accounts["rowan"] = row;

    LoginRequest request = passwordRequest();
    request.freePass = true;

    Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_FALSE(outcome.events().adult);
    // The free-pass projection selects neither column.
    EXPECT_EQ("", outcome.events().ssn);
    EXPECT_EQ("000-000", outcome.events().zipCode);
}

TEST(DecideLogin, ARegistrationNumberWithoutADashIsNeverAdult) {
    // A number with no dash is read as one of the Chinese forms: fifteen
    // digits carry the birthday from offset 6, eighteen from offset 8, and
    // any other length is refused outright. Both substrings keep the digits
    // that follow the birthday, and the comparison is a plain integer one,
    // so the nine- and ten-digit values always exceed the six- or
    // seven-digit threshold.
    const char* const numbers[] = {"110101900615123",    // fifteen
                                   "11010119900615123X", // eighteen
                                   "1234567"};           // neither

    for (unsigned int i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
        FakeLoginAccountRepository repository;
        FakeLoginSession session;

        LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
        row.ssn = numbers[i];
        repository.accounts["rowan"] = row;

        Outcome<LoginAccepted, LoginRejection> outcome =
            decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

        ASSERT_TRUE(outcome.isOk()) << numbers[i];
        EXPECT_FALSE(outcome.events().adult) << numbers[i];
    }
}

TEST(DecideLogin, TheNetMarbleFlagReplacesTheAdultVerdict) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.ssn = "920615-1234567";
    repository.accounts["rowan"] = row;

    LoginRequest request = passwordRequest();
    request.useNetMarbleAdultFlag = true;
    request.netMarbleAdultFlag = false;

    Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_FALSE(outcome.events().adult);
}

TEST(DecideLogin, TheNetMarbleFlagDoesNotReachTheCharacterKickPath) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.logOn = "GAME";
    row.loginIP = "10.0.0.1";
    row.ssn = "920615-1234567";
    repository.accounts["rowan"] = row;

    LoginRequest request = passwordRequest();
    request.useNetMarbleAdultFlag = true;
    request.netMarbleAdultFlag = false;

    Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(LoginNextStep::KickCharacter, outcome.events().next);
    EXPECT_TRUE(outcome.events().adult);
}

// --- decideLogin: the days-left field -------------------------------------

// A paying account whose plan runs to `deadline` and whose family plan runs
// to `familyDeadline`.
Outcome<LoginAccepted, LoginRejection> payingLogin(FakeLoginAccountRepository& repository, FakeLoginSession& session,
                                                   const VSDateTime& now, const VSDateTime& deadline,
                                                   const VSDateTime& familyDeadline) {
    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.payType = 1;
    row.payPlayDate = "irrelevant, the session parses it";
    repository.accounts["rowan"] = row;

    session.payPlayAvailable = deadline;
    session.familyPayPlayAvailable = familyDeadline;

    return decideLogin(passwordRequest(), repository, session, now);
}

TEST(DecideLogin, APlanStillRunningReportsTheDaysLeft) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    Outcome<LoginAccepted, LoginRejection> outcome =
        payingLogin(repository, session, at(2010, 6, 15), at(2010, 6, 20), at(2010, 6, 17));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_FALSE(outcome.events().family);
    EXPECT_EQ(5, outcome.events().lastDays);
}

TEST(DecideLogin, TheFamilyPlanIsReportedWhenItRunsOutLast) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    Outcome<LoginAccepted, LoginRejection> outcome =
        payingLogin(repository, session, at(2010, 6, 15), at(2010, 6, 17), at(2010, 6, 20));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_TRUE(outcome.events().family);
    EXPECT_EQ(5, outcome.events().lastDays);
}

TEST(DecideLogin, APlanIsMeasuredToTheSecond) {
    // One second left is still a running plan, and reports zero days; one
    // second past, with the family plan gone too, reports no expiry at all.
    {
        FakeLoginAccountRepository repository;
        FakeLoginSession session;

        Outcome<LoginAccepted, LoginRejection> outcome = payingLogin(
            repository, session, at(2010, 6, 15, 12, 0, 0), at(2010, 6, 15, 12, 0, 1), at(2010, 6, 15, 11, 59, 59));

        ASSERT_TRUE(outcome.isOk());
        EXPECT_FALSE(outcome.events().family);
        EXPECT_EQ(0, outcome.events().lastDays);
    }

    {
        FakeLoginAccountRepository repository;
        FakeLoginSession session;

        Outcome<LoginAccepted, LoginRejection> outcome = payingLogin(
            repository, session, at(2010, 6, 15, 12, 0, 0), at(2010, 6, 15, 11, 59, 59), at(2010, 6, 15, 11, 59, 59));

        ASSERT_TRUE(outcome.isOk());
        EXPECT_EQ(0xfffe, outcome.events().lastDays);
    }
}

TEST(DecideLogin, AnExpiredPlanStillReportsAFamilyPlanThatRuns) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    Outcome<LoginAccepted, LoginRejection> outcome =
        payingLogin(repository, session, at(2010, 6, 15), at(2010, 6, 10), at(2010, 6, 20));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_TRUE(outcome.events().family);
    EXPECT_EQ(5, outcome.events().lastDays);
}

TEST(DecideLogin, AFreeAccountNeverLooksAtTheDeadlines) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    repository.accounts["rowan"] = FakeLoginAccountRepository::allowedAccount("rowan");
    session.payPlayAvailable = at(2020, 1, 1);
    session.familyPayPlayAvailable = at(2020, 1, 1);

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(0xfffe, outcome.events().lastDays);
}

TEST(DecideLogin, AnUnclaimedComebackWeekIsReportedAndLeftForTheHandlerToWrite) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    repository.accounts["rowan"] = FakeLoginAccountRepository::allowedAccount("rowan");
    repository.unclaimedPremiumEvents.insert("rowan");

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_TRUE(outcome.events().grantPremiumWeek);
    EXPECT_EQ(0xfffd, outcome.events().lastDays);

    EXPECT_TRUE(repository.extendedPayPlay.empty());
    EXPECT_TRUE(repository.premiumEventsReceived.empty());
}

TEST(DecideLogin, TheComebackWeekOverridesADaysLeftCount) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;
    repository.unclaimedPremiumEvents.insert("rowan");

    Outcome<LoginAccepted, LoginRejection> outcome =
        payingLogin(repository, session, at(2010, 6, 15), at(2010, 6, 20), at(2010, 6, 17));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(0xfffd, outcome.events().lastDays);
}

TEST(DecideLogin, TheComebackEventIsNotLookedAtOnAPathThatDoesNotReply) {
    FakeLoginAccountRepository repository;
    FakeLoginSession session;

    LoginAccountRow row = FakeLoginAccountRepository::allowedAccount("rowan");
    row.logOn = "GAME";
    row.loginIP = "10.0.0.1";
    repository.accounts["rowan"] = row;
    repository.unclaimedPremiumEvents.insert("rowan");

    Outcome<LoginAccepted, LoginRejection> outcome =
        decideLogin(passwordRequest(), repository, session, at(2010, 6, 15));

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(LoginNextStep::KickCharacter, outcome.events().next);
    EXPECT_FALSE(outcome.events().grantPremiumWeek);
    EXPECT_EQ(0, repository.hasUnclaimedPremiumEventCalls);
    EXPECT_EQ(session.ownership.account(), nullptr);
}

TEST(DecideLogin, AFailureAfterAcquisitionRetainsOwnershipForDisconnect) {
    class Accounts : public FakeLoginAccountRepository {
    public:
        bool hasUnclaimedPremiumEvent(const std::string&) override {
            throw Error("event lookup failed");
        }
    } repository;
    FakeLoginSession session;
    repository.accounts["rowan"] = FakeLoginAccountRepository::allowedAccount("Rowan");
    EXPECT_THROW(decideLogin(passwordRequest(), repository, session, at(2010, 6, 15)), Error);
    EXPECT_TRUE(session.ownership.owns("Rowan"));
    EXPECT_EQ(repository.markedLoggedOn.size(), 1u);
}

} // namespace
