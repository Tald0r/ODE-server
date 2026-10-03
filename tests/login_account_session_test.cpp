#include <fcntl.h>

#include <array>
#include <cstdlib>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "LoginAccountSession.h"
#include "LoginCompletion.h"
#include "LoginKickRetry.h"
#include "LoginPlayer.h"
#include "Socket.h"
#include "VSDateTime.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"

namespace {

class AccountPlayer : public LoginPlayer {
public:
    AccountPlayer() : LoginPlayer(new Socket()) {
        setID("account");
        setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
    }
    ~AccountPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
};

class Accounts : public FakeLoginAccountRepository {
public:
    bool setLoggedOn(const std::string&) override {
        ++claims;
        return accept;
    }
    void setLoginIP(const std::string&, const std::string&) override {
        if (failIP)
            throw std::runtime_error("IP update failed");
    }
    void markLoggedOff(const std::string& account) override {
        ++logoutAttempts;
        if (beforeLogout)
            beforeLogout(account);
        loggedOff.push_back(account);
    }
    bool accept = true;
    bool failIP = true;
    unsigned claims = 0;
    unsigned logoutAttempts = 0;
    std::function<void(const std::string&)> beforeLogout;
    std::vector<std::string> loggedOff;
};

TEST(LoginAccountSession, PartialCompletionReleasesTheAcquiredRowDespiteWaitingStatus) {
    AccountPlayer player;
    Accounts accounts;
    EXPECT_THROW(de::completeLoginKick(player, accounts, {}, {}), std::runtime_error);
    ASSERT_TRUE(player.loginAccountOwnership().owns("account"));
    de::disconnectLoginPlayer(player, accounts, false, {{}, [] {}});
    EXPECT_EQ(accounts.loggedOff, (std::vector<std::string>{"account"}));
    EXPECT_EQ(player.loginAccountOwnership().account(), nullptr);
}

TEST(LoginAccountSession, AnUnacquiredIdentityNeverLogsOffAnotherSession) {
    AccountPlayer player;
    Accounts accounts;
    player.setPlayerStatus(LPS_BEGIN_SESSION);
    de::disconnectLoginPlayer(player, accounts, false, {{}, [] {}});
    EXPECT_TRUE(accounts.loggedOff.empty());
}

TEST(LoginAccountSession, AFlushFailureStillClosesAndReleasesTheAccount) {
    AccountPlayer player;
    Accounts accounts;
    player.loginAccountOwnership().acquire("account", [] { return true; });
    bool closed = false;
    EXPECT_THROW(de::disconnectLoginPlayer(player, accounts, true,
                                           {[] { throw std::runtime_error("flush failed"); }, [&] { closed = true; }}),
                 std::runtime_error);
    EXPECT_TRUE(closed);
    EXPECT_EQ(accounts.loggedOff, (std::vector<std::string>{"account"}));
    EXPECT_EQ(player.getPlayerStatus(), LPS_END_SESSION);
}

TEST(LoginAccountSession, CleanupUsesTheAcquiredIdentityAfterTheBasePlayerIDChanges) {
    AccountPlayer player;
    Accounts accounts;
    player.loginAccountOwnership().acquire("account", [] { return true; });
    static_cast<Player&>(player).setID("another account");
    player.setPlayerStatus(LPS_PC_MANAGEMENT);
    de::disconnectLoginPlayer(player, accounts, false, {{}, [] {}});
    EXPECT_EQ(accounts.loggedOff, (std::vector<std::string>{"account"}));
}

const auto failures = std::array{std::make_exception_ptr(Error("operation failed")),
                                 std::make_exception_ptr(NoSuchElementException("operation failed")),
                                 std::make_exception_ptr(DatabaseError("operation failed")),
                                 std::make_exception_ptr(std::runtime_error("operation failed")),
                                 std::make_exception_ptr(std::bad_alloc{})};

TEST(LoginAccountOwnership, PublicationFollowsTheWriteAndKeepsAnOwnedIdentity) {
    for (const std::string original : {std::string(), std::string("NONE"), std::string(96, 'a')}) {
        de::LoginAccountOwnership ownership;
        auto source = original;
        EXPECT_TRUE(ownership.acquire(source, [&] {
            EXPECT_EQ(ownership.account(), nullptr);
            source = "changed";
            return true;
        }));
        ASSERT_NE(ownership.account(), nullptr);
        EXPECT_EQ(*ownership.account(), original);
        EXPECT_TRUE(ownership.owns(original));
        EXPECT_FALSE(ownership.owns("changed"));
    }
}

TEST(LoginAccountOwnership, RefusalOrFailureCannotPublishAndTheNextAcquisitionCanSucceed) {
    for (const auto& failure : failures) {
        de::LoginAccountOwnership ownership;
        EXPECT_FALSE(ownership.acquire("account", [] { return false; }));
        EXPECT_EQ(ownership.account(), nullptr);
        std::exception_ptr caught;
        try {
            ownership.acquire("account", [&]() -> bool { std::rethrow_exception(failure); });
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(ownership.account(), nullptr);
        EXPECT_TRUE(ownership.acquire("account", [] { return true; }));
        EXPECT_TRUE(ownership.owns("account"));
    }
}

TEST(LoginAccountOwnership, AnOutstandingOwnerCannotBeReplacedEvenByTheSameAccount) {
    for (const auto* requested : {"account", "another account"}) {
        de::LoginAccountOwnership ownership;
        ASSERT_TRUE(ownership.acquire("account", [] { return true; }));
        const auto* previous = ownership.account();
        unsigned writes = 0;
        EXPECT_THROW(ownership.acquire(requested,
                                       [&] {
                                           ++writes;
                                           return true;
                                       }),
                     DisconnectException);
        EXPECT_EQ(writes, 0u);
        EXPECT_EQ(ownership.account(), previous);
        ASSERT_TRUE(ownership.release([](const std::string&) {}));
        ASSERT_TRUE(ownership.acquire(requested, [&] {
            ++writes;
            return true;
        }));
        EXPECT_TRUE(ownership.owns(requested));
        EXPECT_EQ(writes, 1u);
    }
}

TEST(LoginAccountOwnership, FailedReleaseRetainsTheBorrowedIdentityAndSuccessfulReleaseIsNotRepeated) {
    for (const auto& failure : failures) {
        de::LoginAccountOwnership ownership;
        ASSERT_TRUE(ownership.acquire(std::string(96, 'a'), [] { return true; }));
        const auto* previous = ownership.account();
        std::exception_ptr caught;
        try {
            ownership.release([&](const std::string& account) {
                EXPECT_EQ(&account, previous);
                std::rethrow_exception(failure);
            });
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(ownership.account(), previous);
        unsigned released = 0;
        EXPECT_TRUE(ownership.release([&](const std::string&) { ++released; }));
        EXPECT_FALSE(ownership.release([&](const std::string&) { ++released; }));
        EXPECT_EQ(released, 1u);
        EXPECT_EQ(ownership.account(), nullptr);
    }
}

TEST(LoginAccountSession, EveryUnacquiredPhaseSkipsLogoutRegardlessOfTheIdentity) {
    for (int value = 0; value < PLAYER_STATUS_MAX; ++value) {
        for (const auto* identity : {"", "NONE", "account"}) {
            AccountPlayer player;
            Accounts accounts;
            player.setPlayerStatus(static_cast<PlayerStatus>(value));
            player.setID(identity);
            unsigned closed = 0;
            de::disconnectLoginPlayer(player, accounts, false, {{}, [&] { ++closed; }});
            EXPECT_EQ(closed, 1u);
            EXPECT_EQ(accounts.logoutAttempts, 0u);
            EXPECT_EQ(player.getID(), "NONE");
            EXPECT_EQ(player.getPlayerStatus(), LPS_END_SESSION);
        }
    }
}

TEST(LoginAccountSession, CleanupOrdersFlushCloseAndLogoutBeforeRethrowingTheFirstFailure) {
    for (const auto& failure : failures) {
        for (unsigned mask = 0; mask < 8; ++mask) {
            AccountPlayer player;
            Accounts accounts;
            player.loginAccountOwnership().acquire("account", [] { return true; });
            std::vector<int> steps;
            const auto later = std::make_exception_ptr(std::runtime_error("later failure"));
            const auto firstStage = (mask & 1) ? 1 : (mask & 2) ? 2 : 3;
            const auto step = [&](unsigned flag, int index) {
                steps.push_back(index);
                if (mask & flag)
                    std::rethrow_exception(index == firstStage ? failure : later);
            };
            const de::LoginDisconnectActions actions{[&] { step(1, 1); }, [&] { step(2, 2); }};
            accounts.beforeLogout = [&](const std::string& account) {
                EXPECT_EQ(account, "account");
                EXPECT_EQ(player.getID(), "NONE");
                EXPECT_EQ(player.getPlayerStatus(), LPS_END_SESSION);
                step(4, 3);
            };
            std::exception_ptr caught;
            try {
                de::disconnectLoginPlayer(player, accounts, true, actions);
            } catch (...) {
                caught = std::current_exception();
            }
            EXPECT_EQ(caught, mask ? failure : nullptr);
            EXPECT_EQ(steps, (std::vector<int>{1, 2, 3}));
            EXPECT_EQ(player.loginAccountOwnership().owns("account"), (mask & 4) != 0);
            const auto attempts = accounts.logoutAttempts;
            const bool retryLogout = (mask & 4) != 0;
            // The next call retries close/logout, without flushing END.
            steps.clear();
            accounts.beforeLogout = [&](const std::string&) { steps.push_back(3); };
            de::disconnectLoginPlayer(player, accounts, true, {{}, [&] { steps.push_back(2); }});
            EXPECT_EQ(steps, retryLogout ? std::vector<int>({2, 3}) : std::vector<int>({2}));
            EXPECT_EQ(accounts.logoutAttempts, attempts + (retryLogout ? 1u : 0u));
            EXPECT_EQ(player.loginAccountOwnership().account(), nullptr);
        }
    }
}

TEST(LoginAccountSession, PartialKickCompletionRetriesWithoutTryingToAcquireItsOwnRowAgain) {
    AccountPlayer player;
    Accounts accounts;
    EXPECT_THROW(de::completeLoginKick(player, accounts, {}, {}), std::runtime_error);
    accounts.accept = false; // The database now reads LOGON and would refuse another acquisition.
    accounts.failIP = false;
    EXPECT_TRUE(de::completeLoginKick(player, accounts, [] { return VSDateTime("2026-10-02 12:34:56"); }, {}));
    EXPECT_EQ(accounts.claims, 1u);
    EXPECT_EQ(player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    de::disconnectLoginPlayer(player, accounts, false, {{}, [] {}});
    EXPECT_EQ(accounts.loggedOff, (std::vector<std::string>{"account"}));
}

TEST(LoginAccountSession, AFreshKickCannotDiscardAnOutstandingOwnerOrSendForAnotherAccount) {
    for (const auto* identity : {"account", "another account"}) {
        AccountPlayer player;
        Accounts accounts;
        player.loginAccountOwnership().acquire("account", [] { return true; });
        player.setID(identity);
        unsigned sends = 0;
        EXPECT_THROW(de::beginLoginKick(player,
                                        [&](LoginPlayer&) {
                                            ++sends;
                                            return true;
                                        }),
                     DisconnectException);
        EXPECT_EQ(sends, 0u);
        EXPECT_TRUE(player.loginAccountOwnership().owns("account"));
        de::disconnectLoginPlayer(player, accounts, false, {{}, [] {}});
        EXPECT_EQ(accounts.loggedOff, (std::vector<std::string>{"account"}));
    }
}

TEST(LoginAccountSession, CompletionCannotAcquireAnotherAccountOverAnOutstandingOwner) {
    AccountPlayer player;
    Accounts accounts;
    player.loginAccountOwnership().acquire("first account", [] { return true; });
    EXPECT_THROW(de::completeLoginKick(player, accounts, {}, {}), DisconnectException);
    EXPECT_EQ(accounts.claims, 0u);
    EXPECT_TRUE(player.loginAccountOwnership().owns("first account"));
    de::disconnectLoginPlayer(player, accounts, false, {{}, [] {}});
    EXPECT_EQ(accounts.loggedOff, (std::vector<std::string>{"first account"}));
}

class RegistrationAccounts : public Accounts {
public:
    void insertAccount(const LoginNewAccount& account) override {
        steps.push_back(1);
        correctIdentity &= account.playerID == expected;
        if (failAt == 1)
            std::rethrow_exception(failure);
        ++inserted;
    }
    void markLoggedOnAfterRegister(const std::string& ip, int server, const std::string& account) override {
        steps.push_back(2);
        correctIdentity &= account == expected && ip == "192.0.2.1" && server == 7;
        if (failAt == 2)
            std::rethrow_exception(failure);
        ++registered;
    }
    std::string expected = "new account";
    bool correctIdentity = true;
    std::vector<int> steps;
    unsigned inserted = 0;
    unsigned registered = 0;
    unsigned failAt = 0;
    std::exception_ptr failure;
};

TEST(LoginAccountSession, RegistrationPublishesItsOwnerBeforeAnyReadBackOrReply) {
    AccountPlayer player;
    RegistrationAccounts accounts;
    LoginNewAccount account{};
    account.playerID = accounts.expected;
    de::registerLoginAccount(player.loginAccountOwnership(), accounts, account, "192.0.2.1", 7);
    EXPECT_EQ(accounts.steps, (std::vector<int>{1, 2}));
    EXPECT_TRUE(accounts.correctIdentity);
    EXPECT_TRUE(player.loginAccountOwnership().owns(account.playerID));
    EXPECT_EQ(player.getID(), "account"); // The handler has not published the new ID yet.
    de::disconnectLoginPlayer(player, accounts, false, {{}, [] {}});
    EXPECT_EQ(accounts.loggedOff, (std::vector<std::string>{account.playerID}));
}

TEST(LoginAccountSession, RegistrationFailuresRetainTheirCauseWithoutClaimingAnUnacknowledgedWrite) {
    for (const auto& failure : failures) {
        for (const unsigned stage : {1u, 2u}) {
            de::LoginAccountOwnership ownership;
            RegistrationAccounts accounts;
            LoginNewAccount account{};
            account.playerID = accounts.expected;
            accounts.failAt = stage;
            accounts.failure = failure;
            std::exception_ptr caught;
            try {
                de::registerLoginAccount(ownership, accounts, account, "192.0.2.1", 7);
            } catch (...) {
                caught = std::current_exception();
            }
            EXPECT_EQ(caught, failure);
            EXPECT_EQ(ownership.account(), nullptr);
            EXPECT_EQ(accounts.inserted, stage == 1 ? 0u : 1u);
            EXPECT_EQ(accounts.registered, 0u);
        }
    }
}

TEST(LoginAccountSession, RegistrationCannotInsertOverAnOutstandingAccountOwner) {
    de::LoginAccountOwnership ownership;
    ownership.acquire("previous", [] { return true; });
    RegistrationAccounts accounts;
    LoginNewAccount account{};
    account.playerID = accounts.expected;
    EXPECT_THROW(de::registerLoginAccount(ownership, accounts, account, "192.0.2.1", 7), DisconnectException);
    EXPECT_TRUE(accounts.steps.empty());
    EXPECT_TRUE(ownership.owns("previous"));
}

TEST(LoginAccountSession, BothProductionDisconnectEntryPointsCloseUnacquiredPlayersWithoutDatabaseStartup) {
    for (const bool noLog : {false, true}) {
        AccountPlayer player;
        player.setPlayerStatus(LPS_PC_MANAGEMENT);
        const auto descriptor = player.getSocket()->getSOCKET();
        if (noLog)
            player.disconnect_nolog(DISCONNECTED);
        else
            player.disconnect(DISCONNECTED);
        EXPECT_EQ(::fcntl(descriptor, F_GETFD), -1);
        EXPECT_EQ(player.getPlayerStatus(), LPS_END_SESSION);
        EXPECT_EQ(player.getID(), "NONE");
        EXPECT_NO_THROW(player.disconnect(DISCONNECTED));
    }
}

int checkOwnershipAllocation(std::size_t failAt, bool refuse) {
    de::LoginAccountOwnership ownership;
    const std::string account(96, 'a');
    bool written = false;
    AllocationProbe probe(failAt);
    bool failed = false;
    bool acquired = false;
    try {
        acquired = ownership.acquire(account, [&] {
            const auto storage = std::make_unique<char[]>(96);
            storage[0] = 'x';
            written = !refuse;
            return !refuse;
        });
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed) || acquired != written ||
        ownership.owns(account) != written || probe.outstanding() != (written ? 1u : 0u))
        return 1;
    if (written)
        ownership.release([](const std::string&) {});
    if (probe.outstanding() != 0)
        return 2;
    if (!ownership.acquire(account, [] { return true; }))
        return 3;
    ownership.release([](const std::string&) {});
    return probe.outstanding() == 0 ? 0 : 4;
}

TEST(LoginAccountOwnership, AllocationFailuresPrecedeWritesAndPublicationCannotFailAfterAcknowledgement) {
    for (const bool refuse : {false, true}) {
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(::testing::Message() << refuse << "/" << failAt);
            ASSERT_EXIT(std::_Exit(checkOwnershipAllocation(failAt, refuse)), ::testing::ExitedWithCode(0), "");
        }
    }
}

class CleanupAccounts : public FakeLoginAccountRepository {
public:
    void markLoggedOff(const std::string& account) override {
        if (account != expected)
            std::abort();
        const auto storage = std::make_unique<char[]>(96);
        storage[0] = 'x';
        ++released;
    }
    std::string expected = std::string(96, 'a');
    unsigned released = 0;
};

int checkCleanupAllocation(std::size_t failAt) {
    AccountPlayer player;
    CleanupAccounts accounts;
    player.loginAccountOwnership().acquire(accounts.expected, [] { return true; });
    unsigned closes = 0;
    unsigned flushes = 0;
    const de::LoginDisconnectActions actions{[&] {
                                                 ++flushes;
                                                 const auto storage = std::make_unique<char[]>(96);
                                                 storage[0] = 'x';
                                             },
                                             [&] {
                                                 ++closes;
                                                 const auto storage = std::make_unique<char[]>(96);
                                                 storage[0] = 'x';
                                             }};
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        de::disconnectLoginPlayer(player, accounts, true, actions);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 64 && failed) || probe.outstanding() != 0 || closes != 1 ||
        flushes != 1 || player.getPlayerStatus() != LPS_END_SESSION || player.getID() != "NONE" ||
        player.loginAccountOwnership().owns(accounts.expected) != (accounts.released == 0))
        return 1;
    de::disconnectLoginPlayer(player, accounts, true, actions);
    if (closes != 2 || flushes != 1 || accounts.released != 1 || player.loginAccountOwnership().account())
        return 2;
    return probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginAccountSession, AllocationFailuresStillAttemptAllCleanupAndPermitRetry) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkCleanupAllocation(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

} // namespace
