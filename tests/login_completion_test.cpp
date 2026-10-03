#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "GLKickVerify.h"
#include "LCLoginError.h"
#include "LCLoginOK.h"
#include "LoginCompletion.h"
#include "LoginContext.h"
#include "LoginKickRetry.h"
#include "LoginKickVerification.h"
#include "LoginPlayer.h"
#include "LoginPlayerManager.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "VSDateTime.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginAccountRepository.h"

namespace {

class CompletionPlayer : public LoginPlayer {
public:
    CompletionPlayer() : LoginPlayer(new Socket("192.0.2.7", 9999)) {
        setID("account");
        setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
    }
    ~CompletionPlayer() override {
        setPlayerStatus(LPS_END_SESSION);
    }
    void sendPacket(Packet* reply) override {
        ++sendAttempts;
        if (const auto* error = dynamic_cast<LCLoginError*>(reply)) {
            refused = true;
            errorID = error->getErrorID();
        } else if (const auto* ok = dynamic_cast<LCLoginOK*>(reply)) {
            refused = false;
            adult = ok->isAdult();
            family = ok->isFamily();
            stat = ok->getStat();
            lastDays = ok->getLastDays();
        } else {
            std::abort();
        }
        if (beforeSend)
            beforeSend();
        LoginPlayer::sendPacket(reply);
        ++sent;
    }
    std::string bufferedBytes() const {
        return {m_pOutputStream->getBuffer(), m_pOutputStream->length()};
    }
    const std::string& identity() const {
        return m_ID;
    }
    std::function<void()> beforeSend;
    unsigned sendAttempts = 0;
    unsigned sent = 0;
    bool refused = false;
    BYTE errorID = 0;
    bool adult = false;
    bool family = true;
    BYTE stat = 255;
    WORD lastDays = 0;
};

TEST(LoginCompletion, RefusalSuppressesTheIdentityBeforeReturningToInitialLogin) {
    CompletionPlayer player;
    FakeLoginAccountRepository accounts;
    EXPECT_FALSE(de::completeLoginKick(player, accounts, {}, {}));
    EXPECT_EQ(player.sent, 1u);
    EXPECT_EQ(player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_EQ(player.getID(), "NONE");
    EXPECT_TRUE(player.refused);
    EXPECT_EQ(player.errorID, ALREADY_CONNECTED);
}

TEST(LoginCompletion, BrokenRefusalDiagnosticsCannotPreventTheRefusalReply) {
    CompletionPlayer player;
    FakeLoginAccountRepository accounts;
    bool accepted = true;
    EXPECT_NO_THROW(
        accepted = de::completeLoginKick(
            player, accounts, {},
            {[](const std::string&, const std::string&) { throw std::runtime_error("report failed"); }, {}}));
    EXPECT_FALSE(accepted);
    EXPECT_EQ(player.sent, 1u);
}

class FailingAccounts : public FakeLoginAccountRepository {
public:
    bool setLoggedOn(const std::string&) override {
        std::rethrow_exception(failure);
    }
    std::exception_ptr failure = std::make_exception_ptr(Error("account update failed"));
};

TEST(LoginCompletion, BrokenFailureDiagnosticsCannotReplaceTheOriginalCause) {
    CompletionPlayer player;
    FailingAccounts accounts;
    std::exception_ptr caught;
    try {
        de::completeLoginKick(player, accounts, {},
                              {{}, [](const Throwable&) { throw std::runtime_error("report failed"); }});
    } catch (...) {
        caught = std::current_exception();
    }
    EXPECT_EQ(caught, accounts.failure);
    EXPECT_EQ(player.sent, 0u);
}

enum class Stage { None, Acquire, IP, Send, Clock, Record, RefusalReport, FailureReport };

struct CompletionTrace {
    void observe(Stage stage, PlayerStatus status) {
        if (count == stages.size())
            std::abort();
        stages[count] = stage;
        statuses[count++] = status;
        if (allocate) {
            const auto storage = std::make_unique<char[]>(96);
            storage[0] = 'x';
        }
        if (stage == failureStage)
            std::rethrow_exception(failure);
    }
    std::vector<Stage> events() const {
        return {stages.begin(), stages.begin() + count};
    }
    void clear() {
        count = 0;
    }
    std::array<Stage, 32> stages{};
    std::array<PlayerStatus, 32> statuses{};
    std::size_t count = 0;
    bool allocate = false;
    Stage failureStage = Stage::None;
    std::exception_ptr failure;
};

class CompletionAccounts : public FakeLoginAccountRepository {
public:
    CompletionAccounts(LoginPlayer& player, CompletionTrace& trace) : player(player), trace(trace) {}
    bool setLoggedOn(const std::string& account) override {
        correctAccount &= account == expectedAccount;
        trace.observe(Stage::Acquire, player.getPlayerStatus());
        if (afterAcquire)
            afterAcquire();
        if (accept)
            ++logonWrites;
        return accept;
    }
    void setLoginIP(const std::string& ip, const std::string& account) override {
        correctAccount &= account == expectedAccount;
        correctIP &= ip == "192.0.2.7";
        trace.observe(Stage::IP, player.getPlayerStatus());
        ++ipWrites;
    }
    void insertLoginRecord(const std::string& account, const std::string& ip, const std::string& date,
                           const std::string& time) override {
        correctAccount &= account == expectedAccount;
        correctIP &= ip == "192.0.2.7";
        correctTime &= date == expectedDate && time == expectedTime;
        trace.observe(Stage::Record, player.getPlayerStatus());
        ++records;
    }
    void markLoggedOff(const std::string&) override {
        std::abort();
    }
    LoginPlayer& player;
    CompletionTrace& trace;
    bool accept = true;
    bool correctAccount = true;
    bool correctIP = true;
    bool correctTime = true;
    unsigned logonWrites = 0;
    unsigned ipWrites = 0;
    unsigned records = 0;
    std::string expectedAccount = "account";
    std::string expectedDate = "2026-10-02";
    std::string expectedTime = "23:59:58";
    std::function<void()> afterAcquire;
};

struct Completion {
    Completion() {
        player.beforeSend = [&] { trace.observe(Stage::Send, player.getPlayerStatus()); };
    }
    bool run() {
        return de::completeLoginKick(player, accounts, clock, diagnostics);
    }
    CompletionPlayer player;
    CompletionTrace trace;
    CompletionAccounts accounts{player, trace};
    unsigned clocks = 0;
    unsigned failureReports = 0;
    unsigned refusalReports = 0;
    const Throwable* reportedError = nullptr;
    const VSDateTime timestamp{VSDate(2026, 10, 2), VSTime(23, 59, 58)};
    de::LoginStatisticsClock clock = [&] {
        trace.observe(Stage::Clock, player.getPlayerStatus());
        ++clocks;
        return timestamp;
    };
    de::LoginCompletionDiagnostics diagnostics{[&](const std::string& account, const std::string& ip) {
                                                   accounts.correctAccount &= account == accounts.expectedAccount;
                                                   accounts.correctIP &= ip == "192.0.2.7";
                                                   ++refusalReports;
                                                   trace.observe(Stage::RefusalReport, player.getPlayerStatus());
                                               },
                                               [&](const Throwable& error) {
                                                   ++failureReports;
                                                   reportedError = &error;
                                                   trace.observe(Stage::FailureReport, player.getPlayerStatus());
                                               }};
};

TEST(LoginCompletion, EveryLoginOKFieldIsInitializedAndSerializedForBothLoginPaths) {
    for (const bool adult : {false, true}) {
        for (const bool family : {false, true}) {
            for (const WORD days : {WORD(0), WORD(0x8e9f), WORD(0xfffd), WORD(0xfffe), WORD(0xffff)}) {
                auto reply = de::makeLoginOK(adult, family, days);
                EXPECT_EQ(reply.isAdult(), adult);
                EXPECT_EQ(reply.isFamily(), family);
                EXPECT_EQ(reply.getStat(), 0);
                EXPECT_EQ(reply.getLastDays(), days);
                SocketOutputStream stream(nullptr, 64);
                reply.write(stream);
                ASSERT_EQ(stream.length(), 5u);
                const auto* bytes = reinterpret_cast<const BYTE*>(stream.getBuffer());
                EXPECT_EQ(bytes[0], adult ? 1 : 0);
                EXPECT_EQ(bytes[1], family ? 1 : 0);
                EXPECT_EQ(bytes[2], 0);
                WORD actualDays;
                std::memcpy(&actualDays, bytes + 3, sizeof(actualDays));
                EXPECT_EQ(actualDays, days);
            }
        }
    }
}

TEST(LoginCompletion, SuccessPublishesOnlyAfterSendingAndRecordsTheLoginLast) {
    for (const bool adult : {false, true}) {
        Completion completion;
        completion.player.setAdult(adult);
        completion.player.cacheLoginKickTarget({7, 9, 3, "Rowan"});
        const auto* target = completion.player.getLoginKickTarget();
        ASSERT_TRUE(completion.run());
        EXPECT_EQ(completion.trace.events(),
                  (std::vector{Stage::Acquire, Stage::IP, Stage::Send, Stage::Clock, Stage::Record}));
        for (std::size_t i = 0; i < completion.trace.count; ++i)
            EXPECT_EQ(completion.trace.statuses[i],
                      i < 3 ? LPS_WAITING_FOR_GL_KICK_VERIFY : LPS_WAITING_FOR_CL_GET_PC_LIST);
        EXPECT_EQ(completion.player.sent, 1u);
        EXPECT_EQ(completion.player.adult, adult);
        EXPECT_FALSE(completion.player.family);
        EXPECT_EQ(completion.player.stat, 0);
        EXPECT_EQ(completion.player.lastDays, 0xffff);
        EXPECT_EQ(completion.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
        EXPECT_EQ(completion.player.getID(), "account");
        EXPECT_EQ(completion.player.getLoginKickTarget(), target);
        EXPECT_EQ(completion.accounts.logonWrites, 1u);
        EXPECT_EQ(completion.accounts.ipWrites, 1u);
        EXPECT_EQ(completion.accounts.records, 1u);
        EXPECT_TRUE(completion.accounts.correctAccount && completion.accounts.correctIP &&
                    completion.accounts.correctTime);
        const auto bytes = completion.player.bufferedBytes();
        ASSERT_EQ(bytes.size(), szPacketHeader + 5u);
        EXPECT_EQ(static_cast<BYTE>(bytes[szPacketHeader]), adult ? 1 : 0);
        EXPECT_EQ(bytes.substr(szPacketHeader + 1), std::string("\0\0\xff\xff", 4));
    }
}

TEST(LoginCompletion, RefusalSkipsIPClockAndStatisticsAndPublishesAfterSending) {
    Completion completion;
    completion.accounts.accept = false;
    completion.clock = {};
    EXPECT_FALSE(completion.run());
    EXPECT_EQ(completion.trace.events(), (std::vector{Stage::Acquire, Stage::RefusalReport, Stage::Send}));
    for (std::size_t i = 0; i < completion.trace.count; ++i)
        EXPECT_EQ(completion.trace.statuses[i], LPS_WAITING_FOR_GL_KICK_VERIFY);
    EXPECT_EQ(completion.player.getPlayerStatus(), LPS_BEGIN_SESSION);
    EXPECT_EQ(completion.player.getID(), "NONE");
    EXPECT_EQ(completion.accounts.logonWrites, 0u);
    EXPECT_EQ(completion.accounts.ipWrites, 0u);
    EXPECT_EQ(completion.accounts.records, 0u);
    const auto bytes = completion.player.bufferedBytes();
    ASSERT_EQ(bytes.size(), szPacketHeader + 1u);
    EXPECT_EQ(static_cast<BYTE>(bytes.back()), ALREADY_CONNECTED);
}

TEST(LoginCompletion, RefusedCompletionCanDisconnectWithoutTouchingTheDefaultDatabase) {
    Completion completion;
    completion.accounts.accept = false;
    ASSERT_FALSE(completion.run());
    ASSERT_EQ(completion.player.getID(), "NONE");
    completion.player.disconnect(DISCONNECTED);
    EXPECT_EQ(completion.player.getPlayerStatus(), LPS_END_SESSION);
}

const auto failures = std::array{std::make_exception_ptr(Error("completion failed")),
                                 std::make_exception_ptr(NoSuchElementException("completion failed")),
                                 std::make_exception_ptr(DatabaseError("completion failed")),
                                 std::make_exception_ptr(std::runtime_error("completion failed")),
                                 std::make_exception_ptr(std::bad_alloc{})};

TEST(LoginCompletion, EveryFailureStagePreservesTheCauseAndAlreadyCompletedEffects) {
    for (const auto stage : {Stage::Acquire, Stage::IP, Stage::Send, Stage::Clock, Stage::Record}) {
        for (std::size_t kind = 0; kind < failures.size(); ++kind) {
            Completion completion;
            completion.trace.failureStage = stage;
            completion.trace.failure = failures[kind];
            std::exception_ptr caught;
            try {
                completion.run();
            } catch (...) {
                caught = std::current_exception();
            }
            EXPECT_EQ(caught, failures[kind]);
            const bool sent = stage == Stage::Clock || stage == Stage::Record;
            EXPECT_EQ(completion.accounts.logonWrites, stage == Stage::Acquire ? 0u : 1u);
            EXPECT_EQ(completion.accounts.ipWrites, stage == Stage::Acquire || stage == Stage::IP ? 0u : 1u);
            EXPECT_EQ(completion.player.sent, sent ? 1u : 0u);
            EXPECT_EQ(completion.player.getPlayerStatus(),
                      sent ? LPS_WAITING_FOR_CL_GET_PC_LIST : LPS_WAITING_FOR_GL_KICK_VERIFY);
            EXPECT_EQ(completion.player.getID(), "account");
            EXPECT_EQ(completion.accounts.records, 0u);
            EXPECT_EQ(completion.failureReports, kind < 2 ? 1u : 0u);
            EXPECT_EQ(completion.player.loginAccountOwnership().owns("account"), stage != Stage::Acquire);
            completion.trace.failureStage = Stage::None;
            completion.player.setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
            ASSERT_TRUE(completion.run());
            EXPECT_EQ(completion.accounts.records, 1u);
        }
    }
}

TEST(LoginCompletion, RefusalSendFailuresRetainWaitingIdentityUntilDisconnect) {
    for (const auto& failure : failures) {
        Completion completion;
        completion.accounts.accept = false;
        completion.trace.failureStage = Stage::Send;
        completion.trace.failure = failure;
        std::exception_ptr caught;
        try {
            completion.run();
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(completion.player.sent, 0u);
        EXPECT_EQ(completion.player.getID(), "account");
        ASSERT_EQ(completion.player.getPlayerStatus(), LPS_WAITING_FOR_GL_KICK_VERIFY);
        completion.player.disconnect(DISCONNECTED);
        EXPECT_EQ(completion.player.getID(), "NONE");
        EXPECT_EQ(completion.accounts.records, 0u);
    }
}

TEST(LoginCompletion, EveryDiagnosticFailureLeavesTheOutcomeOrOriginalCauseIntact) {
    for (const auto& reportFailure : failures) {
        Completion refusal;
        refusal.accounts.accept = false;
        refusal.trace.failureStage = Stage::RefusalReport;
        refusal.trace.failure = reportFailure;
        EXPECT_FALSE(refusal.run());
        EXPECT_EQ(refusal.player.sent, 1u);
        EXPECT_EQ(refusal.failureReports, 0u);

        Completion broken;
        const auto original = std::make_exception_ptr(Error("original failure"));
        const Throwable* expected = nullptr;
        try {
            std::rethrow_exception(original);
        } catch (const Throwable& error) {
            expected = &error;
        }
        broken.accounts.afterAcquire = [&] { std::rethrow_exception(original); };
        broken.trace.failureStage = Stage::FailureReport;
        broken.trace.failure = reportFailure;
        std::exception_ptr caught;
        try {
            broken.run();
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, original);
        EXPECT_EQ(broken.reportedError, expected);
        EXPECT_EQ(broken.failureReports, 1u);
    }
}

TEST(LoginCompletion, OwnedReplyInputsSurviveChangesDuringTheRepositoryCall) {
    Completion completion;
    completion.player.setAdult(true);
    completion.accounts.afterAcquire = [&] {
        completion.player.setID("replacement");
        completion.player.setAdult(false);
    };
    ASSERT_TRUE(completion.run());
    EXPECT_TRUE(completion.accounts.correctAccount);
    EXPECT_TRUE(completion.player.adult);
    EXPECT_EQ(completion.player.getID(), "replacement");
}

TEST(LoginCompletion, StatisticsUseTheSuppliedLocalTimestampIncludingLeapDayAndMidnight) {
    for (const auto* text : {"2024-02-29 00:00:00", "2026-10-02 23:59:58", "2000-01-01 12:34:56"}) {
        Completion completion;
        const std::string timestamp(text);
        completion.accounts.expectedDate = timestamp.substr(0, 10);
        completion.accounts.expectedTime = timestamp.substr(11);
        de::recordLogin(completion.accounts, "account", "192.0.2.7", VSDateTime(timestamp));
        EXPECT_EQ(completion.trace.events(), (std::vector{Stage::Record}));
        EXPECT_TRUE(completion.accounts.correctAccount && completion.accounts.correctIP &&
                    completion.accounts.correctTime);
        EXPECT_EQ(completion.accounts.records, 1u);
        EXPECT_EQ(completion.accounts.logonWrites, 0u);
    }
}

TEST(LoginCompletion, StatisticsFailuresRetainTheirRepositoryExceptionWithoutOtherWrites) {
    for (const auto& failure : failures) {
        Completion completion;
        completion.trace.failureStage = Stage::Record;
        completion.trace.failure = failure;
        std::exception_ptr caught;
        try {
            de::recordLogin(completion.accounts, "account", "192.0.2.7", completion.timestamp);
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(caught, failure);
        EXPECT_EQ(completion.accounts.logonWrites, 0u);
        EXPECT_EQ(completion.accounts.records, 0u);
    }
}

TEST(LoginCompletion, TheThirdRetryCanCompleteThroughTheRealRepositoryAndReplyFlow) {
    Completion completion;
    completion.player.cacheLoginKickTarget({7, 9, 3, "Rowan"});
    Timeval now{100, 0};
    const de::LoginKickRequest send = [&](LoginPlayer& player) {
        player.setExpireTimeForKickCharacter(now);
        player.setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
        return true;
    };
    ASSERT_TRUE(de::beginLoginKick(completion.player, send));
    for (unsigned count = 1; count <= 3; ++count) {
        now = completion.player.getExpireTimeForKickCharacter();
        ASSERT_TRUE(de::retryLoginKick(completion.player, now, send, [&](LoginPlayer&) { completion.run(); }));
        EXPECT_EQ(completion.accounts.records, count == 3 ? 1u : 0u);
    }
    EXPECT_EQ(completion.player.getPlayerStatus(), LPS_WAITING_FOR_CL_GET_PC_LIST);
    EXPECT_FALSE(de::retryLoginKick(completion.player, Timeval{999, 0}, {}, {}));
}

TEST(LoginCompletion, VerifiedCompletionRejectsDuplicateRepliesEvenWhenStatisticsFail) {
    for (const bool failStatistics : {false, true}) {
        de::LoginContext context;
        LoginPlayerManager players(context);
        auto incoming = std::make_unique<CompletionPlayer>();
        auto* player = incoming.get();
        player->cacheLoginKickTarget({7, 9, 3, "Rowan"});
        CompletionTrace trace;
        CompletionAccounts accounts(*player, trace);
        if (failStatistics) {
            trace.failureStage = Stage::Record;
            trace.failure = std::make_exception_ptr(Error("statistics failed"));
        }
        players.addPlayer(player);
        incoming.release();
        GLKickVerify packet;
        packet.setID(player->getSocket()->getSOCKET());
        packet.setPCName("Rowan");
        packet.setKicked();
        std::exception_ptr caught;
        bool verified = false;
        try {
            verified = de::verifyLoginKick(players, packet, [&](LoginPlayer& current) {
                de::completeLoginKick(current, accounts, [] { return VSDateTime("2026-10-02 23:59:58"); }, {});
            });
        } catch (...) {
            caught = std::current_exception();
        }
        EXPECT_EQ(verified, !failStatistics);
        EXPECT_EQ(caught, trace.failure);
        EXPECT_FALSE(de::verifyLoginKick(players, packet, {}));
        EXPECT_EQ(accounts.records, failStatistics ? 0u : 1u);
        EXPECT_EQ(player->sent, 1u);
    }
}

int checkAllocationFailure(std::size_t failAt, bool refusal) {
    auto completion = std::make_unique<Completion>();
    const std::string account(96, 'a');
    completion->player.setID(account);
    completion->accounts.expectedAccount = account;
    completion->accounts.accept = !refusal;
    completion->trace.allocate = true;
    AllocationProbe probe(failAt);
    bool failed = false;
    bool accepted = false;
    try {
        accepted = completion->run();
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    // A rejected diagnostic allocation is deliberately suppressed on refusal.
    const auto retained = completion->player.loginAccountOwnership().account() ? 1u : 0u;
    if ((failed && !probe.rejected()) || (failAt == 64 && failed) || probe.outstanding() != retained ||
        accepted != (!failed && !refusal))
        return 1;
    const bool sent = completion->player.sent != 0;
    const auto expectedStatus = !sent     ? LPS_WAITING_FOR_GL_KICK_VERIFY
                                : refusal ? LPS_BEGIN_SESSION
                                          : LPS_WAITING_FOR_CL_GET_PC_LIST;
    if (completion->player.getPlayerStatus() != expectedStatus ||
        completion->player.identity() != (sent && refusal ? std::string("NONE") : account) ||
        completion->accounts.records != (accepted ? 1u : 0u) || !completion->accounts.correctAccount ||
        !completion->accounts.correctIP || !completion->accounts.correctTime)
        return 2;
    if (refusal && (completion->accounts.logonWrites != 0 || completion->accounts.ipWrites != 0))
        return 3;
    completion->player.setID(account);
    completion->player.setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
    completion->trace.clear();
    if (completion->run() != !refusal)
        return 4;
    completion.reset();
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(LoginCompletion, AllocationFailuresReleaseInputsAndPartialReplyStorageBeforeRetry) {
    for (const bool refusal : {false, true}) {
        for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
            SCOPED_TRACE(::testing::Message() << refusal << "/" << failAt);
            ASSERT_EXIT(std::_Exit(checkAllocationFailure(failAt, refusal)), ::testing::ExitedWithCode(0), "");
        }
    }
}

} // namespace
