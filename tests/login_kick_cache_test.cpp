#include <cstdlib>
#include <new>
#include <string>

#include <gtest/gtest.h>

#include "LoginKickCache.h"
#include "support/AllocationProbe.h"

namespace {

TEST(LoginKickCache, EmptyAndClearedOwnersHaveNoTarget) {
    de::LoginKickCache cache;
    EXPECT_EQ(cache.find("account"), nullptr);
    EXPECT_EQ(cache.find(""), nullptr);
    cache.clear();
    cache.store("account", {0, 255, 3, "character"});
    ASSERT_NE(cache.find("account"), nullptr);
    cache.clear();
    cache.clear();
    EXPECT_EQ(cache.find("account"), nullptr);
    cache.store("next", {255, 0, 1, "next character"});
    ASSERT_NE(cache.find("next"), nullptr);
    EXPECT_EQ(*cache.find("next"), (de::LoginKickTarget{255, 0, 1, "next character"}));
}

TEST(LoginKickCache, AccountMatchingUsesTheWholeExactString) {
    de::LoginKickCache cache;
    const std::string account("Account\0suffix", 14);
    cache.store(account, {7, 9, 2, "character"});
    const auto* target = cache.find(account);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(cache.find("Account"), nullptr);
    EXPECT_EQ(cache.find("account"), nullptr);
    EXPECT_EQ(cache.find("Account "), nullptr);
    EXPECT_EQ(cache.find(""), nullptr);
    EXPECT_EQ(cache.find(std::string("Account\0other", 13)), nullptr);
    EXPECT_EQ(cache.find(account), target);
}

TEST(LoginKickCache, StoredAccountAndTargetOwnTheirValues) {
    de::LoginKickCache cache;
    std::string account(64, 'a');
    de::LoginKickTarget target{7, 9, 3, std::string(96, 'c')};
    cache.store(account, target);
    account.assign(80, 'b');
    target = {255, 255, 1, "replacement"};
    const auto* saved = cache.find(std::string(64, 'a'));
    ASSERT_NE(saved, nullptr);
    EXPECT_EQ(*saved, (de::LoginKickTarget{7, 9, 3, std::string(96, 'c')}));
    EXPECT_EQ(cache.find(account), nullptr);
}

TEST(LoginKickCache, SuccessfulReplacementRemovesThePreviousAccountAndEveryField) {
    de::LoginKickCache cache;
    cache.store("first", {255, 255, 3, "first character"});
    cache.store("second", {0, 0, 0, {}});
    EXPECT_EQ(cache.find("first"), nullptr);
    ASSERT_NE(cache.find("second"), nullptr);
    EXPECT_EQ(*cache.find("second"), de::LoginKickTarget{});
    cache.store("second", {7, 9, 1, "second character"});
    EXPECT_EQ(*cache.find("second"), (de::LoginKickTarget{7, 9, 1, "second character"}));
}

TEST(LoginKickCache, ReplacementCanCopyAnEntryBorrowedFromTheSameOwner) {
    de::LoginKickCache cache;
    const de::LoginKickTarget target{7, 9, 3, std::string(96, 'c')};
    cache.store("first", target);
    cache.store("second", *cache.find("first"));
    EXPECT_EQ(cache.find("first"), nullptr);
    ASSERT_NE(cache.find("second"), nullptr);
    EXPECT_EQ(*cache.find("second"), target);
    // Both arguments can borrow the old entry until replacement is prepared.
    cache.store(cache.find("second")->characterName, *cache.find("second"));
    ASSERT_NE(cache.find(target.characterName), nullptr);
    EXPECT_EQ(*cache.find(target.characterName), target);
}

int checkReplacementFailure(std::size_t failAt, bool sameAccount) {
    de::LoginKickCache cache;
    const std::string first(64, 'a');
    const std::string second = sameAccount ? first : std::string(80, 'b');
    const de::LoginKickTarget oldTarget{7, 9, 3, std::string(96, 'c')};
    const de::LoginKickTarget newTarget{255, 0, 1, std::string(128, 'n')};
    cache.store(first, oldTarget);
    const auto* previous = cache.find(first);
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        cache.store(second, newTarget);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || (failAt == 16 && failed))
        return 1;
    if (failed && (cache.find(first) != previous || *previous != oldTarget || probe.outstanding() != 0 ||
                   (!sameAccount && cache.find(second) != nullptr)))
        return 2;
    if (!failed && (!cache.find(second) || *cache.find(second) != newTarget))
        return 3;
    cache.store(second, newTarget);
    if (!cache.find(second) || *cache.find(second) != newTarget || (!sameAccount && cache.find(first)))
        return 4;
    cache.clear();
    return probe.outstanding() == 0 ? 0 : 5;
}

TEST(LoginKickCache, FailedReplacementRetainsBorrowedPointersAndAllowsRetryWithoutLeaks) {
    for (const bool sameAccount : {false, true}) {
        for (std::size_t failAt = 1; failAt <= 16; ++failAt) {
            SCOPED_TRACE(::testing::Message() << sameAccount << "/" << failAt);
            ASSERT_EXIT(std::_Exit(checkReplacementFailure(failAt, sameAccount)), ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST(LoginKickCache, RepeatedReplacementAndDestructionReleaseEveryOwnedEntry) {
    ASSERT_EXIT(
        {
            const std::string account(64, 'a');
            const auto target = (de::LoginKickTarget{7, 9, 3, std::string(96, 'c')});
            AllocationProbe probe;
            for (int owner = 0; owner < 4; ++owner) {
                de::LoginKickCache cache;
                for (int replacement = 0; replacement < 100; ++replacement)
                    cache.store(account, target);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

} // namespace
