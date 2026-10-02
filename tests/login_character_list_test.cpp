#include <array>
#include <cstdlib>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "DatabaseError.h"
#include "LCPCList.h"
#include "LoginCharacterList.h"
#include "support/AllocationProbe.h"
#include "support/FakeLoginCharacterRepository.h"

namespace {

LoginSlayerListRow slayerRow() {
    LoginSlayerListRow row{};
    row.race = "SLAYER";
    row.name = std::string(40, 's');
    row.slot = "SLOT1";
    row.sex = "MALE";
    row.hairColor = 11;
    row.skinColor = 12;
    row.advancementClass = 13;
    row.str = 31;
    row.strExp = 310;
    row.dex = 32;
    row.dexExp = 320;
    row.inte = 33;
    row.intExp = 330;
    row.hp = 144;
    row.currentHP = 71;
    row.mp = 188;
    row.currentMP = 59;
    row.fame = 4321;
    for (int domain = 0; domain < SKILL_DOMAIN_VAMPIRE; ++domain)
        row.domainLevel[domain] = 21 + domain;
    row.alignment = 6000;
    row.shape = PCSlayerInfo::weaponBits(WEAPON_MACE1) | (2u << PCSlayerInfo::SLAYER_BIT_HAIRSTYLE1);
    row.helmetColor = 14;
    row.jacketColor = 15;
    row.pantsColor = 16;
    row.weaponColor = 17;
    row.shieldColor = 18;
    row.rank = 19;
    return row;
}

LoginVampireListRow vampireRow() {
    LoginVampireListRow row{};
    row.name = std::string(40, 'v');
    row.slot = "SLOT2";
    row.sex = "FEMALE";
    row.batColor = 101;
    row.skinColor = 102;
    row.advancementClass = 103;
    row.str = 41;
    row.dex = 42;
    row.inte = 43;
    row.hp = 244;
    row.currentHP = 171;
    row.rank = 14;
    row.goalExp = 556677;
    row.level = 54;
    row.bonus = 55;
    row.fame = 9876;
    row.alignment = 5999;
    row.shape = 5;
    row.coatColor = 106;
    return row;
}

LoginOustersListRow oustersRow() {
    LoginOustersListRow row{};
    row.name = std::string(40, 'o');
    row.slot = "SLOT3";
    row.sex = "MALE";
    row.advancementClass = 23;
    row.str = 51;
    row.dex = 52;
    row.inte = 53;
    row.hp = 344;
    row.currentHP = 271;
    row.rank = 24;
    row.exp = 445566;
    row.level = 64;
    row.bonus = 65;
    row.skillBonus = 66;
    row.fame = 8765;
    row.alignment = 5998;
    row.coatType = 3;
    row.armType = 1;
    row.coatColor = 201;
    row.hairColor = 202;
    row.armColor = 203;
    row.bootsColor = 204;
    return row;
}

class ListCharacters : public FakeLoginCharacterRepository {
public:
    ListCharacters() {
        auto vampireIndex = slayerRow();
        vampireIndex.race = "VAMPIRE";
        vampireIndex.name = "vampire-index";
        auto oustersIndex = slayerRow();
        oustersIndex.race = "OUSTERS";
        oustersIndex.name = "ousters-index";
        slayers = {slayerRow(), vampireIndex, oustersIndex};
    }

    std::vector<LoginSlayerListRow> loadSlayerList(WorldID_t world, const std::string& account) override {
        record('S', world, account);
        return slayers;
    }
    bool loadVampireListRow(WorldID_t world, const std::string& account, const std::string& name,
                            LoginVampireListRow& row) override {
        correctInputs = correctInputs && name == "vampire-index";
        record('V', world, account);
        row = vampire;
        return vampireFound;
    }
    bool loadOustersListRow(WorldID_t world, const std::string& account, const std::string& name,
                            LoginOustersListRow& row) override {
        correctInputs = correctInputs && name == "ousters-index";
        record('O', world, account);
        row = ousters;
        return oustersFound;
    }
    void record(char stage, WorldID_t world, const std::string& account) {
        correctInputs = correctInputs && world == expectedWorld && account == expectedAccount;
        if (readCount < reads.size())
            reads[readCount] = stage;
        ++readCount;
        if (stage == failureStage && failure)
            std::rethrow_exception(failure);
    }

    WorldID_t expectedWorld = 7;
    std::string expectedAccount = std::string(64, 'a');
    bool correctInputs = true;
    std::array<char, 4> reads{};
    unsigned readCount = 0;
    char failureStage = 'S';
    std::exception_ptr failure;
    bool vampireFound = true;
    bool oustersFound = true;
    std::vector<LoginSlayerListRow> slayers;
    LoginVampireListRow vampire = vampireRow();
    LoginOustersListRow ousters = oustersRow();
};

auto build(ListCharacters& characters) {
    return de::makeLoginCharacterList(characters.expectedWorld, characters.expectedAccount, characters);
}

TEST(LoginCharacterList, CompleteRepliesUseExplicitInputsAndRaceSpecificRows) {
    ListCharacters characters;
    const auto reply = build(characters);
    ASSERT_TRUE(reply);
    EXPECT_TRUE(characters.correctInputs);
    EXPECT_EQ(characters.readCount, 3u);
    EXPECT_EQ(characters.reads, (std::array<char, 4>{'S', 'V', 'O', 0}));
    auto* slayer = dynamic_cast<PCSlayerInfo*>(reply->getPCInfo(SLOT1));
    auto* vampire = dynamic_cast<PCVampireInfo*>(reply->getPCInfo(SLOT2));
    auto* ousters = dynamic_cast<PCOustersInfo*>(reply->getPCInfo(SLOT3));
    ASSERT_NE(slayer, nullptr);
    ASSERT_NE(vampire, nullptr);
    ASSERT_NE(ousters, nullptr);
    EXPECT_EQ(slayer->getName(), std::string(20, 's'));
    EXPECT_EQ(vampire->getName(), std::string(20, 'v'));
    EXPECT_EQ(ousters->getName(), std::string(20, 'o'));
    EXPECT_EQ(slayer->getSlot(), SLOT1);
    EXPECT_EQ(vampire->getSlot(), SLOT2);
    EXPECT_EQ(ousters->getSlot(), SLOT3);
    EXPECT_EQ(reply->getPacketSize(), SLOT_MAX + slayer->getSize() + vampire->getSize() + ousters->getSize());
    EXPECT_LE(reply->getPacketSize(), LCPCListFactory::kMaxSize);
}

TEST(LoginCharacterList, SlayerFieldsKeepTheirExistingMappingAndStoredShapePrecedence) {
    ListCharacters characters;
    const auto reply = build(characters);
    const auto* info = dynamic_cast<const PCSlayerInfo*>(reply->getPCInfo(SLOT1));
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->getSTR(), 31);
    EXPECT_EQ(info->getDEX(), 32);
    EXPECT_EQ(info->getINT(), 33);
    EXPECT_EQ(info->getSTRExp(), 310u);
    EXPECT_EQ(info->getDEXExp(), 320u);
    EXPECT_EQ(info->getINTExp(), 330u);
    EXPECT_EQ(info->getAdvancementLevel(), 13);
    EXPECT_EQ(info->getRank(), 19);
    EXPECT_EQ(info->getFame(), 4321u);
    EXPECT_EQ(info->getAlignment(), 6000);
    // Keep the existing database-column argument order for these pairs.
    EXPECT_EQ(info->getHP(ATTR_CURRENT), 144);
    EXPECT_EQ(info->getHP(ATTR_MAX), 71);
    EXPECT_EQ(info->getMP(ATTR_CURRENT), 188);
    EXPECT_EQ(info->getMP(ATTR_MAX), 59);
    for (int domain = 0; domain < SKILL_DOMAIN_VAMPIRE; ++domain)
        EXPECT_EQ(info->getSkillDomainLevel(static_cast<SkillDomain>(domain)), 21 + domain);
    EXPECT_EQ(info->getSex(), FEMALE);
    EXPECT_EQ(info->getHairStyle(), HAIR_STYLE3);
    EXPECT_EQ(info->getWeaponType(), WEAPON_MACE1);
    EXPECT_EQ(info->getHairColor(), 11);
    EXPECT_EQ(info->getSkinColor(), 12);
    EXPECT_EQ(info->getHelmetColor(), 14);
    EXPECT_EQ(info->getJacketColor(), 15);
    EXPECT_EQ(info->getPantsColor(), 16);
    EXPECT_EQ(info->getWeaponColor(), 17);
    EXPECT_EQ(info->getShieldColor(), 18);
}

TEST(LoginCharacterList, VampireFieldsUseTheSecondaryRowAndGoalExperience) {
    ListCharacters characters;
    const auto reply = build(characters);
    const auto* info = dynamic_cast<const PCVampireInfo*>(reply->getPCInfo(SLOT2));
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->getSTR(), 41);
    EXPECT_EQ(info->getDEX(), 42);
    EXPECT_EQ(info->getINT(), 43);
    EXPECT_EQ(info->getHP(ATTR_CURRENT), 244);
    EXPECT_EQ(info->getHP(ATTR_MAX), 171);
    EXPECT_EQ(info->getSex(), FEMALE);
    EXPECT_EQ(info->getBatColor(), 101);
    EXPECT_EQ(info->getSkinColor(), 102);
    EXPECT_EQ(info->getAdvancementLevel(), 103);
    EXPECT_EQ(info->getRank(), 14);
    EXPECT_EQ(info->getExp(), 556677u);
    EXPECT_EQ(info->getLevel(), 54);
    EXPECT_EQ(info->getBonus(), 55);
    EXPECT_EQ(info->getFame(), 9876u);
    EXPECT_EQ(info->getAlignment(), 5999);
    EXPECT_EQ(info->getCoatType(), 5);
    EXPECT_EQ(info->getCoatColor(), 106);
}

TEST(LoginCharacterList, OustersFieldsKeepTheirSecondaryRowAndZeroDefaultMana) {
    ListCharacters characters;
    const auto reply = build(characters);
    const auto* info = dynamic_cast<const PCOustersInfo*>(reply->getPCInfo(SLOT3));
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->getSTR(), 51);
    EXPECT_EQ(info->getDEX(), 52);
    EXPECT_EQ(info->getINT(), 53);
    EXPECT_EQ(info->getHP(ATTR_CURRENT), 344);
    EXPECT_EQ(info->getHP(ATTR_MAX), 271);
    EXPECT_EQ(info->getMP(ATTR_CURRENT), 0);
    EXPECT_EQ(info->getMP(ATTR_MAX), 0);
    EXPECT_EQ(info->getSex(), MALE);
    EXPECT_EQ(info->getAdvancementLevel(), 23);
    EXPECT_EQ(info->getRank(), 24);
    EXPECT_EQ(info->getExp(), 445566u);
    EXPECT_EQ(info->getLevel(), 64);
    EXPECT_EQ(info->getBonus(), 65);
    EXPECT_EQ(info->getSkillBonus(), 66);
    EXPECT_EQ(info->getFame(), 8765u);
    EXPECT_EQ(info->getAlignment(), 5998);
    EXPECT_EQ(static_cast<int>(info->getCoatType()), 3);
    EXPECT_EQ(static_cast<int>(info->getArmType()), 1);
    EXPECT_EQ(info->getCoatColor(), 201);
    EXPECT_EQ(info->getHairColor(), 202);
    EXPECT_EQ(info->getArmColor(), 203);
    EXPECT_EQ(info->getBootsColor(), 204);
}

TEST(LoginCharacterList, EveryQueryReceivesTheExactBoundaryWorldAndAccount) {
    for (const WorldID_t world : {0, 1, 255}) {
        for (const auto& account : {std::string{}, std::string(96, 'a'), std::string("a\0b", 3)}) {
            ListCharacters characters;
            characters.expectedWorld = world;
            characters.expectedAccount = account;
            const auto reply = build(characters);
            EXPECT_TRUE(characters.correctInputs);
            EXPECT_EQ(characters.readCount, 3u);
            EXPECT_EQ(reply->getPCInfo(SLOT3)->getPCType(), PC_OUSTERS);
        }
    }
}

TEST(LoginCharacterList, SecondaryQueriesFollowIndexOrderAndUseTheirOwnSlotAndName) {
    ListCharacters characters;
    std::swap(characters.slayers[0], characters.slayers[2]);
    for (auto& index : characters.slayers) {
        if (index.race != "SLAYER") {
            index.slot = "ignored-index-slot";
            index.sex = "ignored-index-sex";
            index.str = 65535;
        }
    }
    const auto reply = build(characters);
    EXPECT_TRUE(characters.correctInputs);
    EXPECT_EQ(characters.reads, (std::array<char, 4>{'S', 'O', 'V', 0}));
    EXPECT_EQ(reply->getPCInfo(SLOT1)->getPCType(), PC_SLAYER);
    EXPECT_EQ(reply->getPCInfo(SLOT2)->getPCType(), PC_VAMPIRE);
    EXPECT_EQ(reply->getPCInfo(SLOT3)->getPCType(), PC_OUSTERS);
}

TEST(LoginCharacterList, OtherRaceTextsRetainTheOustersLookupPolicy) {
    for (const auto* race : {"OUSTERS", "unknown", "", "slayer"}) {
        ListCharacters characters;
        characters.slayers = {characters.slayers[2]};
        characters.slayers[0].race = race;
        const auto reply = build(characters);
        EXPECT_TRUE(characters.correctInputs);
        EXPECT_EQ(characters.readCount, 2u);
        EXPECT_EQ(characters.reads[1], 'O');
        EXPECT_EQ(reply->getPCInfo(SLOT3)->getPCType(), PC_OUSTERS);
        EXPECT_THROW(reply->getPCInfo(SLOT1), NoSuchElementException);
    }
}

TEST(LoginCharacterList, ThreeMaximumLengthSlayersFitTheFactoryBudget) {
    ListCharacters characters;
    characters.slayers = {slayerRow(), slayerRow(), slayerRow()};
    for (int slot = SLOT1; slot < SLOT_MAX; ++slot)
        characters.slayers[slot].slot = Slot2String[slot];
    const auto reply = build(characters);
    EXPECT_EQ(characters.readCount, 1u);
    EXPECT_EQ(reply->getPacketSize(), LCPCListFactory::kMaxSize);
}

TEST(LoginCharacterList, EveryRaceCanOccupyEveryCanonicalSlot) {
    for (int race = 0; race < 3; ++race) {
        for (int slot = SLOT1; slot < SLOT_MAX; ++slot) {
            ListCharacters characters;
            characters.slayers = {characters.slayers[race]};
            characters.slayers[0].slot = characters.vampire.slot = characters.ousters.slot = Slot2String[slot];
            const auto reply = build(characters);
            EXPECT_EQ(reply->getPCInfo(static_cast<Slot>(slot))->getPCType(), static_cast<PCType>(race));
            EXPECT_EQ(characters.readCount, race == 0 ? 1u : 2u);
            for (int empty = SLOT1; empty < SLOT_MAX; ++empty)
                if (empty != slot)
                    EXPECT_THROW(reply->getPCInfo(static_cast<Slot>(empty)), NoSuchElementException);
        }
    }
}

TEST(LoginCharacterList, EmptyAccountsProduceAnOwnedEmptyReply) {
    ListCharacters characters;
    characters.slayers.clear();
    const auto reply = build(characters);
    EXPECT_EQ(characters.readCount, 1u);
    EXPECT_EQ(reply->getPacketSize(), SLOT_MAX);
    for (const auto slot : {SLOT1, SLOT2, SLOT3})
        EXPECT_THROW(reply->getPCInfo(slot), NoSuchElementException);
}

int checkMalformedRecordCleanup(int race, bool invalidSex) {
    ListCharacters characters;
    auto& field = race == 0   ? (invalidSex ? characters.slayers[0].sex : characters.slayers[0].slot)
                  : race == 1 ? (invalidSex ? characters.vampire.sex : characters.vampire.slot)
                              : (invalidSex ? characters.ousters.sex : characters.ousters.slot);
    field = invalidSex ? "invalid-sex" : "SLOT4";
    AllocationProbe probe;
    bool refused = false;
    try {
        (void)build(characters);
    } catch (const InvalidProtocolException&) {
        refused = true;
    }
    if (!refused || probe.outstanding() != 0)
        return 1;
    field = invalidSex ? "MALE" : Slot2String[race];
    {
        const auto reply = build(characters);
        if (reply->getPCInfo(static_cast<Slot>(race))->getPCType() != static_cast<PCType>(race))
            return 2;
    }
    return probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginCharacterList, InvalidSlotsReleaseEveryPartialRaceRecordAndPermitRetry) {
    for (int race = 0; race < 3; ++race) {
        SCOPED_TRACE(race);
        ASSERT_EXIT(std::_Exit(checkMalformedRecordCleanup(race, false)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginCharacterList, InvalidSexReleasesEveryPartialRaceRecordAndPermitsRetry) {
    for (int race = 0; race < 3; ++race) {
        SCOPED_TRACE(race);
        ASSERT_EXIT(std::_Exit(checkMalformedRecordCleanup(race, true)), ::testing::ExitedWithCode(0), "");
    }
}

int checkDuplicateCleanup(int race) {
    ListCharacters characters;
    if (race == 0)
        characters.slayers.push_back(characters.slayers[0]);
    else if (race == 1)
        characters.vampire.slot = "SLOT1";
    else
        characters.ousters.slot = "SLOT1";
    AllocationProbe probe;
    bool refused = false;
    try {
        (void)build(characters);
    } catch (const DuplicatedException&) {
        refused = true;
    }
    if (!refused || probe.outstanding() != 0)
        return 1;
    if (race == 0)
        characters.slayers.pop_back();
    characters.vampire.slot = "SLOT2";
    characters.ousters.slot = "SLOT3";
    (void)build(characters);
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginCharacterList, DuplicateSlotsReleaseTheIncomingAndPreviouslyAttachedRecords) {
    for (int race = 0; race < 3; ++race) {
        SCOPED_TRACE(race);
        ASSERT_EXIT(std::_Exit(checkDuplicateCleanup(race)), ::testing::ExitedWithCode(0), "");
    }
}

int checkAllocationCleanup(std::size_t failAt) {
    ListCharacters characters;
    AllocationProbe probe(failAt);
    bool failed = false;
    try {
        (void)build(characters);
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    probe.stopFailing();
    if (failed != probe.rejected() || probe.outstanding() != 0 || (failAt == 64 && failed))
        return 1;
    {
        const auto reply = build(characters);
        if (reply->getPCInfo(SLOT3)->getPCType() != PC_OUSTERS || !characters.correctInputs)
            return 2;
    }
    return probe.outstanding() == 0 ? 0 : 3;
}

TEST(LoginCharacterList, AllocationFailuresReleasePartialRepliesAndPermitRetry) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkAllocationCleanup(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginCharacterList, MissingSecondaryRowsKeepTheirDisconnectReasons) {
    for (const bool vampire : {true, false}) {
        ListCharacters characters;
        (vampire ? characters.vampireFound : characters.oustersFound) = false;
        try {
            (void)build(characters);
            FAIL() << "missing race row was accepted";
        } catch (const DisconnectException& error) {
            EXPECT_EQ(error.getMessage(), vampire ? "No Vampire" : "No Ousters");
        }
        EXPECT_EQ(characters.readCount, vampire ? 2u : 3u);
    }
}

int checkMissingRowCleanup(bool vampire) {
    ListCharacters characters;
    auto& found = vampire ? characters.vampireFound : characters.oustersFound;
    found = false;
    AllocationProbe probe;
    bool refused = false;
    try {
        (void)build(characters);
    } catch (const DisconnectException&) {
        refused = true;
    }
    if (!refused || probe.outstanding() != 0)
        return 1;
    found = true;
    (void)build(characters);
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginCharacterList, MissingSecondaryRowsReleaseThePreparedPrefixAndPermitRetry) {
    for (const bool vampire : {true, false})
        ASSERT_EXIT(std::_Exit(checkMissingRowCleanup(vampire)), ::testing::ExitedWithCode(0), "");
}

int checkSetterFailureCleanup(int race) {
    ListCharacters characters;
    auto& attribute = race == 0   ? characters.slayers[0].str
                      : race == 1 ? characters.vampire.str
                                  : characters.ousters.str;
    attribute = 65535;
    AllocationProbe probe;
    bool refused = false;
    try {
        (void)build(characters);
    } catch (const Error& error) {
        refused = error.getMessage() == "STR out of range";
    }
    if (!refused || probe.outstanding() != 0)
        return 1;
    attribute = 30;
    (void)build(characters);
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginCharacterList, AttributeSetterErrorsReleasePartialRecordsAndPermitRetry) {
    for (int race = 0; race < 3; ++race) {
        SCOPED_TRACE(race);
        ASSERT_EXIT(std::_Exit(checkSetterFailureCleanup(race)), ::testing::ExitedWithCode(0), "");
    }
}

int checkQueryFailureCleanup(char stage, int errorKind) {
    ListCharacters characters;
    characters.failureStage = stage;
    if (errorKind == 0)
        characters.failure = std::make_exception_ptr(std::runtime_error("query failed"));
    else if (errorKind == 1)
        characters.failure = std::make_exception_ptr(Error("query failed"));
    else
        characters.failure = std::make_exception_ptr(DatabaseError("query failed"));
    AllocationProbe probe;
    bool correct = false;
    try {
        (void)build(characters);
    } catch (const DisconnectException& error) {
        correct = errorKind == 2 && error.getMessage() == "LoginPlayer::makePCList : query failed";
    } catch (...) {
        correct = errorKind != 2 && std::current_exception() == characters.failure;
    }
    const unsigned expectedReads = stage == 'S' ? 1 : stage == 'V' ? 2 : 3;
    if (!correct || characters.readCount != expectedReads || probe.outstanding() != 0)
        return 1;
    characters.failure = nullptr;
    (void)build(characters);
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginCharacterList, EveryQueryFailureKeepsItsExceptionPolicyReleasesRowsAndPermitsRetry) {
    for (const char stage : {'S', 'V', 'O'}) {
        for (int errorKind = 0; errorKind < 3; ++errorKind) {
            SCOPED_TRACE(::testing::Message() << stage << "/" << errorKind);
            ASSERT_EXIT(std::_Exit(checkQueryFailureCleanup(stage, errorKind)), ::testing::ExitedWithCode(0), "");
        }
    }
}

int checkFailureReportingAllocation(std::size_t failAt) {
    ListCharacters characters;
    characters.failureStage = 'O';
    characters.failure = std::make_exception_ptr(DatabaseError("query failed"));
    AllocationProbe probe(failAt);
    bool allocationFailed = false;
    bool disconnected = false;
    try {
        (void)build(characters);
    } catch (const std::bad_alloc&) {
        allocationFailed = true;
    } catch (const DisconnectException& error) {
        disconnected = error.getMessage() == "LoginPlayer::makePCList : query failed";
    }
    probe.stopFailing();
    if ((!allocationFailed && !disconnected) || allocationFailed != probe.rejected() || probe.outstanding() != 0 ||
        (failAt == 64 && allocationFailed))
        return 1;
    characters.failure = nullptr;
    (void)build(characters);
    return probe.outstanding() == 0 ? 0 : 2;
}

TEST(LoginCharacterList, AllocationFailuresDuringQueryErrorTranslationStillReleaseThePartialReply) {
    for (std::size_t failAt = 1; failAt <= 64; ++failAt) {
        SCOPED_TRACE(failAt);
        ASSERT_EXIT(std::_Exit(checkFailureReportingAllocation(failAt)), ::testing::ExitedWithCode(0), "");
    }
}

TEST(LoginCharacterList, ReturnedRepliesRemainIndependentOfRepositoryChangesAndLaterFailures) {
    ListCharacters characters;
    const auto first = build(characters);
    const auto* firstSlayer = first->getPCInfo(SLOT1);
    const auto* firstVampire = first->getPCInfo(SLOT2);
    characters.vampire.slot = "SLOT1";
    EXPECT_THROW((void)build(characters), DuplicatedException);
    EXPECT_EQ(first->getPCInfo(SLOT1), firstSlayer);
    EXPECT_EQ(first->getPCInfo(SLOT2), firstVampire);
    characters.slayers.clear();
    const auto empty = build(characters);
    EXPECT_EQ(empty->getPacketSize(), SLOT_MAX);
    EXPECT_EQ(first->getPCInfo(SLOT3)->getPCType(), PC_OUSTERS);
}

int checkRepeatedCleanup() {
    ListCharacters characters;
    AllocationProbe probe;
    for (int repeat = 0; repeat < 32; ++repeat) {
        (void)build(characters);
        if (probe.outstanding() != 0)
            return 1;
    }
    return 0;
}

TEST(LoginCharacterList, RepeatedSuccessfulRepliesReleaseAllOwnedStorage) {
    ASSERT_EXIT(std::_Exit(checkRepeatedCleanup()), ::testing::ExitedWithCode(0), "");
}

} // namespace
