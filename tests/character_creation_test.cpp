// The loginserver's character-creation decision
// (src/server/loginserver/CharacterCreation.cpp): every rejection with the
// input that triggers it, the precedence between them, and the rows an
// accepted creation produces. The repository is a fake, so no database is
// involved; the MySQL-backed integration tier
// (tests/integration/mysql_loginserver_repository_test.cpp) is the
// authority on what the real repository answers.

#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "CharacterCreation.h"
#include "Exception.h"
#include "FakeLoginCharacterRepository.h"

namespace {

const CreatePCActions kActions{+[] { return 0u; }, {}};

// A request that is accepted as-is by a repository with no rows: a
// slayer with 10/10/10.
CreatePCRequest slayerRequest(const std::string& name = "Rowan") {
    CreatePCRequest request;
    request.worldID = 1;
    request.serverGroupID = 2;
    request.playerID = "account";
    request.name = name;
    request.slot = SLOT2;
    request.sex = MALE;
    request.hairStyle = HAIR_STYLE3;
    request.hairColor = 7;
    request.skinColor = 9;
    request.str = 10;
    request.dex = 10;
    request.inte = 10;
    request.race = RACE_SLAYER;
    return request;
}

CreatePCRequest vampireRequest() {
    CreatePCRequest request = slayerRequest("Nosfer");
    request.race = RACE_VAMPIRE;
    request.str = 20;
    request.dex = 20;
    request.inte = 20;
    return request;
}

CreatePCRequest oustersRequest() {
    CreatePCRequest request = slayerRequest("Sylph");
    request.race = RACE_OUSTERS;
    request.str = 15;
    request.dex = 15;
    request.inte = 15;
    return request;
}

// Balance rows for the levels the requests above land on.
void fillBalance(FakeLoginCharacterRepository& repository) {
    repository.rankGoalExp[0] = 1000;
    repository.rankGoalExp[1] = 1100;
    repository.rankGoalExp[2] = 1200;
    repository.vampireGoalExp[1] = 2000;
    repository.oustersGoalExp[1] = 3000;

    for (int attr = 0; attr < LOGIN_ATTR_TABLE_MAX; attr++) {
        for (int level = 0; level <= 20; level++) {
            repository.attrGoalExp[std::make_pair(attr, level)] = 100 * (attr + 1) + level;
            repository.attrAccumExp[std::make_pair(attr, level)] = 10000 * (attr + 1) + level;
        }
    }
}

// --- rejections -----------------------------------------------------------

TEST(DecideCreatePC, ReservedNameIsRefusedBeforeAnyRepositoryRead) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;

    Outcome<CreatedCharacter, CreatePCRejection> outcome =
        decideCreatePC(slayerRequest("theGMan"), repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::ReservedName, outcome.rejection());
    EXPECT_EQ(0, repository.slayerNameExistsCalls);
}

TEST(DecideCreatePC, EveryReservedTokenIsRefusedAsASubstring) {
    const char* const names[] = {"NONE",   "xNONEx", "GM",     "aGMb",   "관리자", "도우미",
                                 "담당자", "운영",   "기획자", "개발자", "테스터", "직원"};

    for (unsigned int i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;

        Outcome<CreatedCharacter, CreatePCRejection> outcome =
            decideCreatePC(slayerRequest(names[i]), repository, balance, kActions);

        ASSERT_TRUE(outcome.isRejected()) << names[i];
        EXPECT_EQ(CreatePCRejection::ReservedName, outcome.rejection()) << names[i];
    }
}

TEST(DecideCreatePC, ATakenNameIsRefused) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    repository.existingNames.insert("Rowan");

    Outcome<CreatedCharacter, CreatePCRejection> outcome =
        decideCreatePC(slayerRequest(), repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::NameTaken, outcome.rejection());
    EXPECT_EQ(0, repository.slotOccupiedCalls);
}

TEST(DecideCreatePC, AnOccupiedSlotIsRefused) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    repository.occupiedSlots.insert(std::make_pair(std::string("account"), std::string("SLOT2")));

    Outcome<CreatedCharacter, CreatePCRejection> outcome =
        decideCreatePC(slayerRequest(), repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::SlotOccupied, outcome.rejection());
    EXPECT_EQ(0, repository.rankGoalExpCalls);
}

TEST(DecideCreatePC, AnotherSlotOfTheSameAccountIsFree) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);
    repository.occupiedSlots.insert(std::make_pair(std::string("account"), std::string("SLOT1")));

    Outcome<CreatedCharacter, CreatePCRejection> outcome =
        decideCreatePC(slayerRequest(), repository, balance, kActions);

    EXPECT_TRUE(outcome.isOk());
}

TEST(DecideCreatePC, ASlotOutsideTheThreeSlotsIsRefused) {
    // Slot2String has one entry per slot; CLCreatePC::read passes the byte
    // through unchecked, so the decision is what keeps the index in range.
    const int slots[] = {SLOT_MAX, 4, 17, 255};

    for (unsigned int i = 0; i < sizeof(slots) / sizeof(slots[0]); i++) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        fillBalance(repository);

        CreatePCRequest request = slayerRequest();
        request.slot = slots[i];

        Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

        ASSERT_TRUE(outcome.isRejected()) << slots[i];
        EXPECT_EQ(CreatePCRejection::InvalidSlot, outcome.rejection()) << slots[i];
        // The refusal comes before the slot probe, which is the first
        // reader of the table.
        EXPECT_EQ(0, repository.slotOccupiedCalls) << slots[i];
    }
}

TEST(DecideCreatePC, EveryRealSlotIsAccepted) {
    const Slot slots[] = {SLOT1, SLOT2, SLOT3};
    const char* const texts[] = {"SLOT1", "SLOT2", "SLOT3"};

    for (int i = 0; i < 3; i++) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        fillBalance(repository);

        CreatePCRequest request = slayerRequest();
        request.slot = slots[i];

        Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

        ASSERT_TRUE(outcome.isOk()) << texts[i];
        EXPECT_EQ(texts[i], outcome.events().slayer.slot);
    }
}

TEST(DecideCreatePC, AHairStyleOutsideTheThreeStylesIsRefused) {
    // CLCreatePC::getHairStyle() masks two bits, so a crafted packet can
    // reach 3 — one past the end of HairStyle2String.
    const int hairStyles[] = {3, 4, 255};

    for (unsigned int i = 0; i < sizeof(hairStyles) / sizeof(hairStyles[0]); i++) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        fillBalance(repository);

        CreatePCRequest request = slayerRequest();
        request.hairStyle = hairStyles[i];

        Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

        ASSERT_TRUE(outcome.isRejected()) << hairStyles[i];
        EXPECT_EQ(CreatePCRejection::InvalidHairStyle, outcome.rejection()) << hairStyles[i];
        EXPECT_EQ(0, repository.slotOccupiedCalls) << hairStyles[i];
    }
}

TEST(DecideCreatePC, EveryRealHairStyleIsAccepted) {
    const HairStyle hairStyles[] = {HAIR_STYLE1, HAIR_STYLE2, HAIR_STYLE3};
    const char* const texts[] = {"HAIR_STYLE1", "HAIR_STYLE2", "HAIR_STYLE3"};

    for (int i = 0; i < 3; i++) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        fillBalance(repository);

        CreatePCRequest request = slayerRequest();
        request.hairStyle = hairStyles[i];

        Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

        ASSERT_TRUE(outcome.isOk()) << texts[i];
        EXPECT_EQ(texts[i], outcome.events().slayer.hairStyle);
    }
}

TEST(DecideCreatePC, SlayerAttributesOutsideFiveToTwentyAreRefused) {
    struct Case {
        int str;
        int dex;
        int inte;
    };
    const Case cases[] = {{4, 10, 10}, {21, 5, 5}, {10, 4, 10}, {5, 21, 5}, {10, 10, 4}, {5, 5, 21}};

    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        fillBalance(repository);

        CreatePCRequest request = slayerRequest();
        request.str = cases[i].str;
        request.dex = cases[i].dex;
        request.inte = cases[i].inte;

        Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

        ASSERT_TRUE(outcome.isRejected()) << i;
        EXPECT_EQ(CreatePCRejection::InvalidAttributes, outcome.rejection()) << i;
    }
}

TEST(DecideCreatePC, SlayerAttributesSummingAboveThirtyAreRefused) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = slayerRequest();
    request.str = 20;
    request.dex = 6;
    request.inte = 5;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::InvalidAttributes, outcome.rejection());
}

TEST(DecideCreatePC, SlayerAttributesSummingBelowThirtyAreAccepted) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = slayerRequest();
    request.str = 5;
    request.dex = 5;
    request.inte = 5;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    EXPECT_TRUE(outcome.isOk());
}

TEST(DecideCreatePC, AVampireThatIsNotTwentyTwentyTwentyIsRefused) {
    const Attr_t attrs[3][3] = {{19, 20, 20}, {20, 19, 20}, {20, 20, 19}};

    for (int i = 0; i < 3; i++) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        fillBalance(repository);

        CreatePCRequest request = vampireRequest();
        request.str = attrs[i][0];
        request.dex = attrs[i][1];
        request.inte = attrs[i][2];

        Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

        ASSERT_TRUE(outcome.isRejected()) << i;
        EXPECT_EQ(CreatePCRejection::InvalidAttributes, outcome.rejection()) << i;
    }
}

TEST(DecideCreatePC, OustersAttributesNotSummingToFortyFiveAreRefused) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = oustersRequest();
    request.inte = 16;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::InvalidAttributes, outcome.rejection());
}

TEST(DecideCreatePC, OustersBelowTenAreAcceptedWhenTheySumToFortyFive) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = oustersRequest();
    request.str = 25;
    request.dex = 15;
    request.inte = 5;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(25, outcome.events().slayer.str);
    EXPECT_EQ(5, outcome.events().ousters.inte);
}

TEST(DecideCreatePC, AnUnknownRaceIsRefused) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = slayerRequest();
    request.race = 3;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::UnknownRace, outcome.rejection());
}

TEST(DecideCreatePC, AnUnknownRaceSkipsTheAttributeRulesEntirely) {
    // No race branch claims the request, so nothing sets the invalid flag
    // and the race is what refuses it.
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = slayerRequest();
    request.race = 250;
    request.str = 1;
    request.dex = 1;
    request.inte = 1;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::UnknownRace, outcome.rejection());
}

// --- precedence -----------------------------------------------------------

TEST(DecideCreatePC, ReservedNameWinsOverEveryOtherReason) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    repository.existingNames.insert("GMan");
    repository.occupiedSlots.insert(std::make_pair(std::string("account"), std::string("SLOT2")));

    CreatePCRequest request = slayerRequest("GMan");
    request.race = 3;
    request.str = 99;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::ReservedName, outcome.rejection());
}

TEST(DecideCreatePC, ATakenNameWinsOverAnOccupiedSlot) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    repository.existingNames.insert("Rowan");
    repository.occupiedSlots.insert(std::make_pair(std::string("account"), std::string("SLOT2")));

    Outcome<CreatedCharacter, CreatePCRejection> outcome =
        decideCreatePC(slayerRequest(), repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::NameTaken, outcome.rejection());
}

TEST(DecideCreatePC, ATakenNameWinsOverAnOutOfRangeSlot) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    repository.existingNames.insert("Rowan");

    CreatePCRequest request = slayerRequest();
    request.slot = 9;
    request.hairStyle = 9;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::NameTaken, outcome.rejection());
}

TEST(DecideCreatePC, AnOutOfRangeSlotWinsOverAnOutOfRangeHairStyle) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;

    CreatePCRequest request = slayerRequest();
    request.slot = 9;
    request.hairStyle = 9;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::InvalidSlot, outcome.rejection());
}

TEST(DecideCreatePC, AnOutOfRangeSlotWinsOverAnOccupiedSlotAndInvalidAttributes) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    repository.occupiedSlots.insert(std::make_pair(std::string("account"), std::string("SLOT2")));

    CreatePCRequest request = slayerRequest();
    request.slot = 9;
    request.str = 99;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::InvalidSlot, outcome.rejection());
}

TEST(DecideCreatePC, AnOccupiedSlotWinsOverInvalidAttributes) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    repository.occupiedSlots.insert(std::make_pair(std::string("account"), std::string("SLOT2")));

    CreatePCRequest request = slayerRequest();
    request.str = 1;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::SlotOccupied, outcome.rejection());
}

TEST(DecideCreatePC, InvalidAttributesWinOverAnUnknownRaceWhenTheRaceIsAKnownOne) {
    // Only a known race can produce invalid attributes, so the two never
    // compete on the same request; this pins that a valid vampire is not
    // rejected as an unknown race on its way through the roll.
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = vampireRequest();
    request.str = 19;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(CreatePCRejection::InvalidAttributes, outcome.rejection());
}

// --- the accepted character -----------------------------------------------

TEST(DecideCreatePC, ASlayerGetsASlayerRowAVampireRowAndTheSlayerFlagSet) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    Outcome<CreatedCharacter, CreatePCRejection> outcome =
        decideCreatePC(slayerRequest(), repository, balance, kActions);

    ASSERT_TRUE(outcome.isOk());
    const CreatedCharacter& created = outcome.events();

    EXPECT_EQ(10, created.str);
    EXPECT_EQ(10, created.dex);
    EXPECT_EQ(10, created.inte);
    EXPECT_FALSE(created.hasOustersRow);
    EXPECT_EQ(LOGIN_FLAGSET_SLAYER, created.flagSet);

    EXPECT_EQ("SLAYER", created.slayer.race);
    EXPECT_EQ("Rowan", created.slayer.name);
    EXPECT_EQ("account", created.slayer.playerID);
    EXPECT_EQ("SLOT2", created.slayer.slot);
    EXPECT_EQ(2, created.slayer.serverGroupID);
    EXPECT_EQ("MALE", created.slayer.sex);
    EXPECT_EQ("HAIR_STYLE3", created.slayer.hairStyle);
    EXPECT_EQ(7, created.slayer.hairColor);
    EXPECT_EQ(9, created.slayer.skinColor);

    // The exp columns come from the balance tables at the attribute's own
    // level, and the accumulated column one level below it.
    EXPECT_EQ(110, created.slayer.strGoalExp);
    EXPECT_EQ(10009, created.slayer.strExp);
    EXPECT_EQ(210, created.slayer.dexGoalExp);
    EXPECT_EQ(20009, created.slayer.dexExp);
    EXPECT_EQ(310, created.slayer.intGoalExp);
    EXPECT_EQ(30009, created.slayer.intExp);

    EXPECT_EQ(1, created.slayer.rank);
    EXPECT_EQ(0, created.slayer.rankExp);
    EXPECT_EQ(1000, created.slayer.rankGoalExp);
    EXPECT_EQ(20, created.slayer.hp);
    EXPECT_EQ(20, created.slayer.currentHP);
    EXPECT_EQ(20, created.slayer.mp);
    EXPECT_EQ(20, created.slayer.currentMP);

    // Sex in bit 0, hair style in bits 1-2: MALE plus HAIR_STYLE3 is
    // 1 | (2 << 1).
    EXPECT_EQ(5u, created.slayer.shape);
    EXPECT_EQ(0, created.slayer.helmetColor);
    EXPECT_EQ(0, created.slayer.jacketColor);
    EXPECT_EQ(0, created.slayer.pantsColor);
    EXPECT_EQ(0, created.slayer.weaponColor);
    EXPECT_EQ(0, created.slayer.shieldColor);

    // The Vampire row a non-Ousters character also gets carries only the
    // sex bit in its shape.
    EXPECT_EQ("Rowan", created.vampire.name);
    EXPECT_EQ("account", created.vampire.playerID);
    EXPECT_EQ("SLOT2", created.vampire.slot);
    EXPECT_EQ("MALE", created.vampire.sex);
    EXPECT_EQ(9, created.vampire.skinColor);
    EXPECT_EQ(2000, created.vampire.goalExp);
    EXPECT_EQ(1100, created.vampire.rankGoalExp);
    EXPECT_EQ(1u, created.vampire.shape);
}

TEST(DecideCreatePC, AFemaleSlayerHasNoSexBit) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = slayerRequest();
    request.sex = FEMALE;
    request.hairStyle = HAIR_STYLE1;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ("FEMALE", outcome.events().slayer.sex);
    EXPECT_EQ(0u, outcome.events().slayer.shape);
    EXPECT_EQ(0u, outcome.events().vampire.shape);
}

TEST(DecideCreatePC, EveryLegalVampireRollUsesTwoDrawsBeforeAttributeReads) {
    for (unsigned str = 5; str <= 20; ++str) {
        for (unsigned dex = 5; dex <= 25 - str; ++dex) {
            SCOPED_TRACE(::testing::Message() << str << "/" << dex);
            FakeLoginCharacterRepository repository;
            CreatePCBalanceCache balance;
            fillBalance(repository);
            unsigned draws = 0;
            const CreatePCActions actions{[&] {
                                              EXPECT_EQ(repository.rankGoalExpCalls, 3);
                                              EXPECT_EQ(repository.vampireGoalExpCalls, 1);
                                              EXPECT_EQ(repository.oustersGoalExpCalls, 1);
                                              EXPECT_EQ(repository.attrGoalExpCalls, 0);
                                              EXPECT_EQ(repository.attrAccumExpCalls, 0);
                                              return draws++ == 0 ? str - 5 : dex - 5;
                                          },
                                          {}};
            auto outcome = decideCreatePC(vampireRequest(), repository, balance, actions);
            ASSERT_TRUE(outcome.isOk());
            const auto& created = outcome.events();
            const unsigned inte = 30 - str - dex;
            EXPECT_EQ(draws, 2u);
            EXPECT_EQ(created.str, str);
            EXPECT_EQ(created.dex, dex);
            EXPECT_EQ(created.inte, inte);
            EXPECT_EQ(created.slayer.race, "VAMPIRE");
            EXPECT_EQ(created.slayer.str, str);
            EXPECT_EQ(created.slayer.dex, dex);
            EXPECT_EQ(created.slayer.inte, inte);
            EXPECT_EQ(created.slayer.strGoalExp, 100 + str);
            EXPECT_EQ(created.slayer.dexGoalExp, 200 + dex);
            EXPECT_EQ(created.slayer.intGoalExp, 300 + inte);
            EXPECT_EQ(created.slayer.strExp, 10000 + str - 1);
            EXPECT_EQ(created.slayer.dexExp, 20000 + dex - 1);
            EXPECT_EQ(created.slayer.intExp, 30000 + inte - 1);
            EXPECT_EQ(created.slayer.hp, str * 2);
            EXPECT_EQ(created.slayer.mp, inte * 2);
            EXPECT_FALSE(created.hasOustersRow);
            EXPECT_EQ(created.flagSet, LOGIN_FLAGSET_OTHER);
        }
    }
}

TEST(DecideCreatePC, AnOustersGetsAnOustersRowInsteadOfAVampireOne) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    Outcome<CreatedCharacter, CreatePCRejection> outcome =
        decideCreatePC(oustersRequest(), repository, balance, kActions);

    ASSERT_TRUE(outcome.isOk());
    const CreatedCharacter& created = outcome.events();

    EXPECT_TRUE(created.hasOustersRow);
    EXPECT_EQ(LOGIN_FLAGSET_OTHER, created.flagSet);
    EXPECT_EQ("OUSTERS", created.slayer.race);

    EXPECT_EQ("Sylph", created.ousters.name);
    EXPECT_EQ("account", created.ousters.playerID);
    EXPECT_EQ("SLOT2", created.ousters.slot);
    EXPECT_EQ(2, created.ousters.serverGroupID);
    EXPECT_EQ(15, created.ousters.str);
    EXPECT_EQ(15, created.ousters.dex);
    EXPECT_EQ(15, created.ousters.inte);
    EXPECT_EQ(3000, created.ousters.goalExp);
    EXPECT_EQ(1200, created.ousters.rankGoalExp);
    EXPECT_EQ(7, created.ousters.hairColor);
}

TEST(DecideCreatePC, AnOustersWithAZeroAttributeReadsTheLevelBelowZero) {
    // 45/0/0 sums to 45, so it is accepted; the accumulated-exp probe for
    // a zero attribute asks for level -1, which no table has.
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    CreatePCRequest request = oustersRequest();
    request.str = 45;
    request.dex = 0;
    request.inte = 0;

    Outcome<CreatedCharacter, CreatePCRejection> outcome = decideCreatePC(request, repository, balance, kActions);

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(0, outcome.events().slayer.dexExp);
    EXPECT_EQ(0, outcome.events().slayer.intExp);
}

// --- the balance cache ----------------------------------------------------

TEST(CreatePCBalanceCache, TheLevelOneGoalsAreReadOnce) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    Outcome<CreatedCharacter, CreatePCRejection> first =
        decideCreatePC(slayerRequest("Rowan"), repository, balance, kActions);
    ASSERT_TRUE(first.isOk());

    CreatePCRequest second = slayerRequest("Briar");
    second.slot = SLOT3;
    Outcome<CreatedCharacter, CreatePCRejection> secondOutcome = decideCreatePC(second, repository, balance, kActions);
    ASSERT_TRUE(secondOutcome.isOk());

    // Three rank types, read once each.
    EXPECT_EQ(3, repository.rankGoalExpCalls);
    EXPECT_EQ(1, repository.vampireGoalExpCalls);
    EXPECT_EQ(1, repository.oustersGoalExpCalls);
    // STR, DEX and INT at level 10, read once each.
    EXPECT_EQ(3, repository.attrGoalExpCalls);
    EXPECT_EQ(3, repository.attrAccumExpCalls);
}

TEST(CreatePCBalanceCache, ADifferentLevelIsANewRead) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);

    Outcome<CreatedCharacter, CreatePCRejection> first =
        decideCreatePC(slayerRequest("Rowan"), repository, balance, kActions);
    ASSERT_TRUE(first.isOk());

    CreatePCRequest second = slayerRequest("Briar");
    second.slot = SLOT3;
    second.str = 11;
    second.dex = 9;
    Outcome<CreatedCharacter, CreatePCRejection> secondOutcome = decideCreatePC(second, repository, balance, kActions);
    ASSERT_TRUE(secondOutcome.isOk());

    // STR 11 and DEX 9 are new levels; INT stays at 10.
    EXPECT_EQ(5, repository.attrGoalExpCalls);
    EXPECT_EQ(5, repository.attrAccumExpCalls);
}

TEST(CreatePCBalanceCache, AMissingRankRowLeavesMinusOneAndIsRetried) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    fillBalance(repository);
    repository.rankGoalExp.erase(0);

    Outcome<CreatedCharacter, CreatePCRejection> first =
        decideCreatePC(slayerRequest("Rowan"), repository, balance, kActions);
    ASSERT_TRUE(first.isOk());
    EXPECT_EQ(-1, first.events().slayer.rankGoalExp);

    CreatePCRequest second = slayerRequest("Briar");
    second.slot = SLOT3;
    Outcome<CreatedCharacter, CreatePCRejection> secondOutcome = decideCreatePC(second, repository, balance, kActions);
    ASSERT_TRUE(secondOutcome.isOk());

    // The Slayer rank row was asked for again; the two that answered were
    // not.
    EXPECT_EQ(4, repository.rankGoalExpCalls);
}

TEST(CreatePCBalanceCache, AMissingAttributeRowLeavesZero) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    repository.rankGoalExp[0] = 1000;

    Outcome<CreatedCharacter, CreatePCRejection> outcome =
        decideCreatePC(slayerRequest(), repository, balance, kActions);

    ASSERT_TRUE(outcome.isOk());
    EXPECT_EQ(0, outcome.events().slayer.strGoalExp);
    EXPECT_EQ(0, outcome.events().slayer.strExp);
    EXPECT_EQ(-1, outcome.events().vampire.goalExp);
    EXPECT_EQ(-1, outcome.events().vampire.rankGoalExp);
}

// --- the name filter shared with CLQueryCharacterNameHandler --------------

TEST(IsAvailableID, AnOrdinaryNameIsAvailable) {
    EXPECT_TRUE(isAvailableID("Rowan"));
    EXPECT_TRUE(isAvailableID(""));
    EXPECT_TRUE(isAvailableID("gm"));
}

TEST(IsAvailableID, AReservedTokenAnywhereInTheNameIsNot) {
    EXPECT_FALSE(isAvailableID("NONE"));
    EXPECT_FALSE(isAvailableID("aGMb"));
    EXPECT_FALSE(isAvailableID("직원1"));
}

TEST(DecideCreatePC, ReportingFailureCannotRejectLowOustersWhoseAttributesStillSumToFortyFive) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    auto request = oustersRequest();
    request.str = 5;
    request.dex = 20;
    request.inte = 20;
    const CreatePCActions actions{{}, [](const CreatePCRequest&) { throw std::runtime_error("report failed"); }};
    EXPECT_NO_THROW(EXPECT_TRUE(decideCreatePC(request, repository, balance, actions).isOk()));
}

TEST(DecideCreatePC, ReportingFailureCannotHideAnInvalidAttributeRejection) {
    FakeLoginCharacterRepository repository;
    CreatePCBalanceCache balance;
    auto request = oustersRequest();
    request.str = 5;
    const CreatePCActions actions{{}, [](const CreatePCRequest&) { throw std::bad_alloc(); }};
    EXPECT_NO_THROW(EXPECT_EQ(decideCreatePC(request, repository, balance, actions).rejection(),
                              CreatePCRejection::InvalidAttributes));
}

TEST(DecideCreatePC, RawDrawsPreserveModuloReductionIncludingTheSingleChoiceSecondDraw) {
    struct Sample {
        unsigned first;
        unsigned second;
        int str;
        int dex;
        int inte;
    };
    const Sample samples[] = {{16, 16, 5, 5, 20},
                              {32, 31, 5, 20, 5},
                              {31, 123, 20, 5, 5},
                              {std::numeric_limits<unsigned>::max(), std::numeric_limits<unsigned>::max(), 20, 5, 5}};
    for (const auto& sample : samples) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        unsigned draws = 0;
        const CreatePCActions actions{[&] { return draws++ == 0 ? sample.first : sample.second; }, {}};
        auto result = decideCreatePC(vampireRequest(), repository, balance, actions);
        ASSERT_TRUE(result.isOk());
        EXPECT_EQ(result.events().str, sample.str);
        EXPECT_EQ(result.events().dex, sample.dex);
        EXPECT_EQ(result.events().inte, sample.inte);
        EXPECT_EQ(draws, 2u);
    }
}

TEST(DecideCreatePC, RandomDrawsAreSkippedForEarlierRefusalsAndOtherRaces) {
    for (int gate = 0; gate < 8; ++gate) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        auto request = vampireRequest();
        if (gate == 0)
            request.name = "NONE";
        if (gate == 1)
            repository.existingNames.insert(request.name);
        if (gate == 2)
            request.slot = SLOT_MAX;
        if (gate == 3)
            request.hairStyle = 3;
        if (gate == 4)
            repository.occupiedSlots.insert({request.playerID, "SLOT2"});
        if (gate == 5)
            request.str = 19;
        if (gate == 6)
            request = slayerRequest();
        if (gate == 7)
            request = oustersRequest();
        unsigned draws = 0;
        const CreatePCActions actions{[&] {
                                          ++draws;
                                          return 0u;
                                      },
                                      {}};
        auto result = decideCreatePC(request, repository, balance, actions);
        EXPECT_EQ(result.isOk(), gate >= 6);
        EXPECT_EQ(draws, 0u);
    }
}

TEST(DecideCreatePC, RandomFailuresKeepTheirIdentityAndDoNotRewindEarlierCacheFills) {
    const std::exception_ptr failures[] = {std::make_exception_ptr(Error("random unavailable")),
                                           std::make_exception_ptr(std::runtime_error("draw failed")),
                                           std::make_exception_ptr(std::bad_alloc())};
    for (const auto& failure : failures) {
        for (unsigned failAt : {1u, 2u}) {
            FakeLoginCharacterRepository repository;
            CreatePCBalanceCache balance;
            fillBalance(repository);
            unsigned draws = 0;
            const CreatePCActions actions{[&] {
                                              if (++draws == failAt)
                                                  std::rethrow_exception(failure);
                                              return 0u;
                                          },
                                          {}};
            try {
                (void)decideCreatePC(vampireRequest(), repository, balance, actions);
                FAIL() << "random failure was swallowed";
            } catch (...) {
                EXPECT_EQ(std::current_exception(), failure);
            }
            EXPECT_EQ(draws, failAt);
            EXPECT_EQ(repository.rankGoalExpCalls, 3);
            EXPECT_EQ(repository.attrGoalExpCalls, 0);
            EXPECT_TRUE(decideCreatePC(vampireRequest(), repository, balance, actions).isOk());
            EXPECT_EQ(draws, failAt + 2);
            EXPECT_EQ(repository.rankGoalExpCalls, 3);
        }
    }
}

TEST(DecideCreatePC, OnlyLowOustersAreReportedOnceBeforeAttributeValidation) {
    struct Attributes {
        Attr_t str;
        Attr_t dex;
        Attr_t inte;
        bool report;
        bool accepted;
    };
    const Attributes cases[] = {{9, 16, 20, true, true},  {16, 9, 20, true, true},   {20, 16, 9, true, true},
                                {9, 9, 27, true, true},   {10, 15, 20, false, true}, {0, 0, 45, true, true},
                                {9, 15, 20, true, false}, {15, 15, 15, false, true}};
    for (const auto& input : cases) {
        FakeLoginCharacterRepository repository;
        CreatePCBalanceCache balance;
        fillBalance(repository);
        auto request = oustersRequest();
        request.str = input.str;
        request.dex = input.dex;
        request.inte = input.inte;
        unsigned reports = 0;
        const CreatePCActions actions{{}, [&](const CreatePCRequest& reported) {
                                          ++reports;
                                          EXPECT_EQ(reported.playerID, "account");
                                          EXPECT_EQ(reported.name, "Sylph");
                                          EXPECT_EQ(reported.worldID, 1);
                                          EXPECT_EQ(reported.serverGroupID, 2);
                                          EXPECT_EQ(reported.str, input.str);
                                          EXPECT_EQ(reported.dex, input.dex);
                                          EXPECT_EQ(reported.inte, input.inte);
                                          EXPECT_EQ(repository.rankGoalExpCalls, 3);
                                          EXPECT_EQ(repository.attrGoalExpCalls, 0);
                                      }};
        auto result = decideCreatePC(request, repository, balance, actions);
        EXPECT_EQ(result.isOk(), input.accepted);
        EXPECT_EQ(reports, input.report ? 1u : 0u);
    }
}

TEST(DecideCreatePC, AFailedReportCannotHideTheNextRepositoryFailure) {
    class FailedAttributes : public FakeLoginCharacterRepository {
    public:
        bool loadAttrGoalExp(WorldID_t, LoginAttrTable, int, int&) override {
            std::rethrow_exception(failure);
        }
        std::exception_ptr failure = std::make_exception_ptr(Error("attribute lookup failed"));
    } repository;
    CreatePCBalanceCache balance;
    auto request = oustersRequest();
    request.str = 5;
    request.dex = 20;
    request.inte = 20;
    const CreatePCActions actions{{}, [](const CreatePCRequest&) { throw NoSuchElementException("report failed"); }};
    try {
        (void)decideCreatePC(request, repository, balance, actions);
        FAIL() << "attribute failure was swallowed";
    } catch (...) {
        EXPECT_EQ(std::current_exception(), repository.failure);
    }
}

} // namespace
