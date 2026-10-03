// The loginserver's account-registration decision
// (src/server/loginserver/Registration.cpp): every refusal with the input
// that triggers it, the precedence between them, and the Player row an
// accepted registration hands to the caller. The repository is a fake, so
// no database is involved; the MySQL-backed integration tier
// (tests/integration/mysql_loginserver_repository_test.cpp) is the
// authority on what the real repository answers.
//
// The password hashing is the real argon2id, which costs 64 MiB per call,
// so the cases that reach it are kept few. Neither a password nor a hash is
// ever passed to a gtest matcher that would print it on failure.

#include <cstddef>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "Exception.h"
#include "FakeLoginAccountRepository.h"
#include "PasswordHash.h"
#include "Registration.h"

namespace {

const RegisterPlayerActions kActions{de::password::hash};

const char* const kPassword = "correct horse";

// A request every field of which is acceptable.
RegisterPlayerRequest goodRequest() {
    RegisterPlayerRequest request;
    request.playerID = "newcomer";
    request.password = kPassword;
    request.name = "Ada Lovelace";
    request.sex = FEMALE;
    request.ssn = "800101-1234567";
    request.telephone = "02-000-0000";
    request.cellular = "010-0000-0000";
    request.zipCode = "123-456";
    request.address = "1 Analytical Engine Way";
    request.nation = 1;
    request.email = "ada@example.com";
    request.homepage = "http://example.com";
    request.profile = "counts things";
    request.publicProfile = true;
    return request;
}

// Refuses without asking the repository anything, and — for everything
// decided before the hashing — without paying for an argon2 call.
void expectRefusedBeforeAnyRead(const RegisterPlayerRequest& request, RegisterPlayerRejection reason) {
    FakeLoginAccountRepository repository;

    Outcome<LoginNewAccount, RegisterPlayerRefusal> outcome = decideRegisterPlayer(request, repository, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(reason, outcome.rejection().reason);
    EXPECT_EQ(0, repository.accountExistsCalls);
    EXPECT_TRUE(repository.insertedAccounts.empty());
}

// --- the validation refusals ----------------------------------------------

TEST(DecideRegisterPlayer, AnEmptyIDIsRefused) {
    RegisterPlayerRequest request = goodRequest();
    request.playerID = "";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::EmptyID);
}

TEST(DecideRegisterPlayer, AnIDShorterThanFourCharactersIsRefused) {
    RegisterPlayerRequest request = goodRequest();
    request.playerID = "abc";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::ShortID);
}

TEST(DecideRegisterPlayer, TheShortestAcceptedIDIsFourCharacters) {
    RegisterPlayerRequest request = goodRequest();
    request.playerID = "abcd";
    // Not refused for its length: the next thing it meets is the hashing,
    // then the id probe, and an unknown id is accepted.
    FakeLoginAccountRepository repository;

    Outcome<LoginNewAccount, RegisterPlayerRefusal> outcome = decideRegisterPlayer(request, repository, kActions);

    EXPECT_TRUE(outcome.isOk());
}

TEST(DecideRegisterPlayer, EverySqlMetaCharacterInTheIDIsRefused) {
    const char* const ids[] = {"new'comer", "new\\comer", "new\"comer", "new;comer"};

    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        RegisterPlayerRequest request = goodRequest();
        request.playerID = ids[i];
        expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::InvalidID);
    }
}

TEST(DecideRegisterPlayer, AnEmptyPasswordIsRefused) {
    RegisterPlayerRequest request = goodRequest();
    request.password = "";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::EmptyPassword);
}

TEST(DecideRegisterPlayer, APasswordShorterThanSixCharactersIsRefused) {
    RegisterPlayerRequest request = goodRequest();
    request.password = "abcde";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::ShortPassword);
}

TEST(DecideRegisterPlayer, AnEmptyNameIsRefused) {
    RegisterPlayerRequest request = goodRequest();
    request.name = "";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::EmptyName);
}

TEST(DecideRegisterPlayer, AnEmptyRegistrationNumberIsRefused) {
    RegisterPlayerRequest request = goodRequest();
    request.ssn = "";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::EmptySSN);
}

TEST(DecideRegisterPlayer, EveryProfileFieldIsCheckedForSqlMetaCharacters) {
    RegisterPlayerRequest fields[9];
    for (size_t i = 0; i < 9; i++)
        fields[i] = goodRequest();

    fields[0].name += "'";
    fields[1].ssn += "'";
    fields[2].telephone += "'";
    fields[3].cellular += "'";
    fields[4].zipCode += "'";
    fields[5].address += "'";
    fields[6].email += "'";
    fields[7].homepage += "'";
    fields[8].profile += "'";

    for (size_t i = 0; i < 9; i++)
        expectRefusedBeforeAnyRead(fields[i], RegisterPlayerRejection::InvalidProfileField);
}

TEST(DecideRegisterPlayer, TheIDIsCheckedBeforeThePassword) {
    RegisterPlayerRequest request = goodRequest();
    request.playerID = "";
    request.password = "";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::EmptyID);
}

TEST(DecideRegisterPlayer, ThePasswordIsCheckedBeforeTheName) {
    RegisterPlayerRequest request = goodRequest();
    request.password = "";
    request.name = "";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::EmptyPassword);
}

TEST(DecideRegisterPlayer, AnEmptyNameIsRefusedForBeingEmptyNotForItsCharacters) {
    RegisterPlayerRequest request = goodRequest();
    request.name = "";
    request.profile += "'";
    expectRefusedBeforeAnyRead(request, RegisterPlayerRejection::EmptyName);
}

// --- the id probe -----------------------------------------------------------

TEST(DecideRegisterPlayer, AnIDThatIsTakenIsRefused) {
    FakeLoginAccountRepository repository;
    repository.registeredIDs.insert("newcomer");

    Outcome<LoginNewAccount, RegisterPlayerRefusal> outcome = decideRegisterPlayer(goodRequest(), repository, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(RegisterPlayerRejection::AlreadyRegistered, outcome.rejection().reason);
    // The decision writes nothing: the INSERT is the caller's.
    EXPECT_TRUE(repository.insertedAccounts.empty());
}

TEST(DecideRegisterPlayer, TheIDProbeRunsOnceAndOnlyAfterTheValidation) {
    FakeLoginAccountRepository repository;

    Outcome<LoginNewAccount, RegisterPlayerRefusal> outcome = decideRegisterPlayer(goodRequest(), repository, kActions);

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(1, repository.accountExistsCalls);
}

// --- an accepted registration ----------------------------------------------

TEST(DecideRegisterPlayer, TheAcceptedRowCarriesThePacketsFields) {
    FakeLoginAccountRepository repository;
    const RegisterPlayerRequest request = goodRequest();

    Outcome<LoginNewAccount, RegisterPlayerRefusal> outcome = decideRegisterPlayer(request, repository, kActions);

    ASSERT_TRUE(outcome.isOk());
    const LoginNewAccount account = std::move(outcome).events();

    EXPECT_EQ("newcomer", account.playerID);
    EXPECT_EQ("Ada Lovelace", account.name);
    EXPECT_EQ("FEMALE", account.sex);
    EXPECT_EQ("800101-1234567", account.ssn);
    EXPECT_EQ("02-000-0000", account.telephone);
    EXPECT_EQ("010-0000-0000", account.cellular);
    EXPECT_EQ("123-456", account.zipCode);
    EXPECT_EQ("1 Analytical Engine Way", account.address);
    EXPECT_EQ(1, account.nation);
    EXPECT_EQ("ada@example.com", account.email);
    EXPECT_EQ("http://example.com", account.homepage);
    EXPECT_EQ("counts things", account.profile);
    EXPECT_EQ("PUBLIC", account.pub);
}

TEST(DecideRegisterPlayer, TheStoredPasswordIsAnArgon2idHashOfWhatWasSent) {
    FakeLoginAccountRepository repository;
    const RegisterPlayerRequest request = goodRequest();

    Outcome<LoginNewAccount, RegisterPlayerRefusal> outcome = decideRegisterPlayer(request, repository, kActions);

    ASSERT_TRUE(outcome.isOk());
    const LoginNewAccount account = std::move(outcome).events();

    // Neither value is handed to a matcher that would print it.
    EXPECT_FALSE(account.password == request.password);
    EXPECT_TRUE(de::password::isHashed(account.password));
    EXPECT_TRUE(de::password::verify(account.password, request.password) == de::password::Verify::Accepted);
    EXPECT_TRUE(de::password::verify(account.password, "wrong password") == de::password::Verify::Rejected);
}

TEST(DecideRegisterPlayer, APrivateProfileIsSpelledPRIVATE) {
    FakeLoginAccountRepository repository;
    RegisterPlayerRequest request = goodRequest();
    request.publicProfile = false;
    request.sex = MALE;

    Outcome<LoginNewAccount, RegisterPlayerRefusal> outcome = decideRegisterPlayer(request, repository, kActions);

    ASSERT_TRUE(outcome.isOk());
    const LoginNewAccount account = std::move(outcome).events();
    EXPECT_EQ("PRIVATE", account.pub);
    EXPECT_EQ("MALE", account.sex);
}

TEST(DecideRegisterPlayer, InvalidSexIsRefusedBeforeHashingOrReading) {
    for (int value : {-1, 2, 3, 255}) {
        FakeLoginAccountRepository repository;
        auto request = goodRequest();
        request.sex = value;
        unsigned hashes = 0;
        const RegisterPlayerActions actions{[&](const std::string&) -> std::string {
            ++hashes;
            throw std::runtime_error("hash should not run");
        }};
        const auto result = decideRegisterPlayer(request, repository, actions);
        ASSERT_TRUE(result.isRejected());
        EXPECT_EQ(result.rejection().reason, RegisterPlayerRejection::InvalidProfileField);
        EXPECT_EQ(hashes, 0u);
        EXPECT_EQ(repository.accountExistsCalls, 0);
    }
}

TEST(DecideRegisterPlayer, TheExplicitHasherRunsOnceBeforeTheIDProbeEvenForAnExistingID) {
    for (bool exists : {false, true}) {
        FakeLoginAccountRepository repository;
        if (exists)
            repository.registeredIDs.insert("newcomer");
        unsigned hashes = 0;
        const RegisterPlayerActions actions{[&](const std::string& password) {
            EXPECT_TRUE(password == kPassword);
            EXPECT_EQ(repository.accountExistsCalls, 0);
            ++hashes;
            return std::string("encoded-test-hash");
        }};
        const auto result = decideRegisterPlayer(goodRequest(), repository, actions);
        EXPECT_EQ(hashes, 1u);
        EXPECT_EQ(repository.accountExistsCalls, 1);
        EXPECT_EQ(result.isRejected(), exists);
        if (result.isOk())
            EXPECT_TRUE(result.events().password == "encoded-test-hash");
    }
}

TEST(DecideRegisterPlayer, StandardHashFailuresRemainRefusalsBeforeTheRepositoryProbe) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(std::runtime_error("synthetic hashing failure")),
                                           std::make_exception_ptr(std::bad_alloc()),
                                           std::make_exception_ptr(Error("synthetic throwable"))};
    for (const auto& failure : failures) {
        FakeLoginAccountRepository repository;
        const RegisterPlayerActions actions{
            [&](const std::string&) -> std::string { std::rethrow_exception(failure); }};
        const auto result = decideRegisterPlayer(goodRequest(), repository, actions);
        ASSERT_TRUE(result.isRejected());
        EXPECT_EQ(result.rejection().reason, RegisterPlayerRejection::PasswordHashingFailed);
        EXPECT_FALSE(result.rejection().detail.empty());
        EXPECT_EQ(repository.accountExistsCalls, 0);
    }
}

TEST(DecideRegisterPlayer, NonstandardHashFailuresKeepTheirIdentity) {
    struct HashFailure {};
    const auto failure = std::make_exception_ptr(HashFailure{});
    FakeLoginAccountRepository repository;
    const RegisterPlayerActions actions{[&](const std::string&) -> std::string { std::rethrow_exception(failure); }};
    try {
        (void)decideRegisterPlayer(goodRequest(), repository, actions);
        FAIL() << "hash failure was swallowed";
    } catch (...) {
        EXPECT_EQ(std::current_exception(), failure);
    }
    EXPECT_EQ(repository.accountExistsCalls, 0);
}

TEST(DecideRegisterPlayer, RepositoryExceptionsAreNotConvertedIntoHashingRefusals) {
    class Accounts : public FakeLoginAccountRepository {
    public:
        bool accountExists(const std::string&) override {
            std::rethrow_exception(failure);
        }
        std::exception_ptr failure = std::make_exception_ptr(std::runtime_error("probe failed"));
    } repository;
    const RegisterPlayerActions actions{[](const std::string&) { return std::string("encoded-test-hash"); }};
    try {
        (void)decideRegisterPlayer(goodRequest(), repository, actions);
        FAIL() << "probe failure was swallowed";
    } catch (...) {
        EXPECT_EQ(std::current_exception(), repository.failure);
    }
}

} // namespace
