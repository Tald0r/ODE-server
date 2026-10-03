// The loginserver's character-selection decision
// (src/server/loginserver/CharacterSelection.cpp): every rejection with the
// input that triggers it, the precedence between them, and the routing an
// accepted selection produces. The repository and the server topology are
// fakes, so neither a database nor a game server is involved; the
// MySQL-backed integration tier
// (tests/integration/mysql_loginserver_repository_test.cpp) is the
// authority on what the real repository answers.

#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "CharacterSelection.h"
#include "Exception.h"
#include "FakeLoginCharacterRepository.h"

namespace {

// The server tables a selection reads. Nothing is a non-PK server and
// every zone is on kDefaultServerID unless a test says otherwise.
class FakeSelectPCTopology : public SelectPCTopology {
public:
    static constexpr ServerID_t kDefaultServerID = 7;

    bool nonPKServer = false;
    ServerID_t serverID = kDefaultServerID;

    int isNonPKServerCalls = 0;
    std::vector<ZoneID_t> zoneServerIDCalls;

    bool isNonPKServer(WorldID_t, ServerGroupID_t) override {
        isNonPKServerCalls++;
        return nonPKServer;
    }

    ServerID_t zoneServerID(ZoneID_t zoneID) override {
        zoneServerIDCalls.push_back(zoneID);
        return serverID;
    }
};

// A request that is accepted as-is once the repository holds the matching
// character.
SelectPCRequest slayerRequest() {
    SelectPCRequest request;
    request.worldID = 1;
    request.serverGroupID = 2;
    request.playerID = "account";
    request.pcName = "Rowan";
    request.pcType = PC_SLAYER;
    request.inCharacterManagement = true;
    return request;
}

// The character the request above names: in an ordinary zone, slot 2,
// level 30, competence 1.
void addRowan(FakeLoginCharacterRepository& repository) {
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", 30, 1);
}

// --- rejections -----------------------------------------------------------

TEST(DecideSelectPC, AnAccountThatHasNotAgreedIsRefusedBeforeAnyRead) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    addRowan(repository);

    SelectPCRequest request = slayerRequest();
    request.agreedToTerms = false;

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::DidNotAgree, outcome.rejection());
    EXPECT_EQ(0, repository.loadCharacterForSelectCalls);
}

TEST(DecideSelectPC, ASessionOutsideCharacterManagementIsRefusedBeforeAnyRead) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    addRowan(repository);

    SelectPCRequest request = slayerRequest();
    request.inCharacterManagement = false;

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::InvalidStatus, outcome.rejection());
    EXPECT_EQ(0, repository.loadCharacterForSelectCalls);
}

TEST(DecideSelectPC, ANameThatIsNotOnTheAccountIsRefused) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    addRowan(repository);

    SelectPCRequest request = slayerRequest();
    request.pcName = "Briar";

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::NoSuchCharacter, outcome.rejection());
}

TEST(DecideSelectPC, AnotherAccountsCharacterIsRefused) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    addRowan(repository);

    SelectPCRequest request = slayerRequest();
    request.playerID = "someone-else";

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::NoSuchCharacter, outcome.rejection());
}

TEST(DecideSelectPC, TheRaceTableComesFromThePacketsPCType) {
    // The Slayer row is only visible to a PC_SLAYER request, and so on for
    // the other two: asking for the wrong table is "no such PC".
    const PCType types[] = {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS};
    const LoginRaceTable tables[] = {LOGIN_RACE_TABLE_SLAYER, LOGIN_RACE_TABLE_VAMPIRE, LOGIN_RACE_TABLE_OUSTERS};

    for (int stored = 0; stored < 3; stored++) {
        for (int asked = 0; asked < 3; asked++) {
            FakeLoginCharacterRepository repository;
            FakeSelectPCTopology topology;
            repository.addSelectableCharacter(tables[stored], "account", "Rowan", 2101, "SLOT2", 30, 1);

            SelectPCRequest request = slayerRequest();
            request.pcType = types[asked];

            Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

            if (stored == asked) {
                ASSERT_TRUE(outcome.isOk()) << stored << "/" << asked;
                EXPECT_EQ(tables[stored], outcome.events().table);
            } else {
                ASSERT_TRUE(outcome.isRejected()) << stored << "/" << asked;
                EXPECT_EQ(SelectPCRejection::NoSuchCharacter, outcome.rejection());
            }
        }
    }
}

TEST(DecideSelectPC, TheFreePlayCapIsOffUnlessTheRequestAsksForIt) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", 9999, 1);

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

    EXPECT_TRUE(outcome.isOk());
}

TEST(DecideSelectPC, ASlayerAboveTheFreePlayDomainSumIsRefused) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", 41, 1);

    SelectPCRequest request = slayerRequest();
    request.checkFreePlayLimit = true;
    request.freePlaySlayerDomainSum = 40;
    request.freePlayVampireLevel = 100;

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::FreePlayLimit, outcome.rejection());
}

TEST(DecideSelectPC, ASlayerExactlyAtTheFreePlayDomainSumIsAccepted) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", 40, 1);

    SelectPCRequest request = slayerRequest();
    request.checkFreePlayLimit = true;
    request.freePlaySlayerDomainSum = 40;
    request.freePlayVampireLevel = 1;

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    EXPECT_TRUE(outcome.isOk());
}

TEST(DecideSelectPC, TheOtherTwoRacesAreCappedByLevelNotByDomainSum) {
    const PCType types[] = {PC_VAMPIRE, PC_OUSTERS};
    const LoginRaceTable tables[] = {LOGIN_RACE_TABLE_VAMPIRE, LOGIN_RACE_TABLE_OUSTERS};

    for (int i = 0; i < 2; i++) {
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        repository.addSelectableCharacter(tables[i], "account", "Rowan", 2101, "SLOT2", 31, 1);

        SelectPCRequest request = slayerRequest();
        request.pcType = types[i];
        request.checkFreePlayLimit = true;
        // A domain sum high enough to pass would not save a vampire: the
        // level limit is the one that applies.
        request.freePlaySlayerDomainSum = 100;
        request.freePlayVampireLevel = 30;

        Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

        ASSERT_TRUE(outcome.isRejected()) << i;
        EXPECT_EQ(SelectPCRejection::FreePlayLimit, outcome.rejection()) << i;
    }
}

TEST(DecideSelectPC, ANonPKServerRefusesAHighLevelCharacterOfTheTopCompetence) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    topology.nonPKServer = true;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", 81, 3);

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::NonPKServerLimit, outcome.rejection());
}

TEST(DecideSelectPC, ANonPKServerAcceptsEitherHalfOfThatPairAlone) {
    struct Case {
        int level;
        int competence;
    };
    // Level 80 is not above the bound, and any competence but the top one
    // passes at any level.
    const Case cases[] = {{80, 3}, {81, 2}, {80, 2}, {200, 0}};

    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        topology.nonPKServer = true;
        repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", cases[i].level,
                                          cases[i].competence);

        Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

        EXPECT_TRUE(outcome.isOk()) << i;
    }
}

TEST(DecideSelectPC, APKServerAcceptsTheSameCharacter) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    topology.nonPKServer = false;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", 81, 3);

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

    EXPECT_TRUE(outcome.isOk());
}

TEST(DecideSelectPC, ASlotTextThatIsNotFiveCharactersIsRefused) {
    const char* const slots[] = {"", "SLOT", "SLOT10", "SLOT_2"};

    for (unsigned int i = 0; i < sizeof(slots) / sizeof(slots[0]); i++) {
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, slots[i], 30, 1);

        Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

        ASSERT_TRUE(outcome.isRejected()) << slots[i];
        EXPECT_EQ(SelectPCRejection::NoSlot, outcome.rejection()) << slots[i];
    }
}

TEST(DecideSelectPC, MalformedPrefixesAreRefusedForEveryRaceAndRoutingPath) {
    const PCType types[] = {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS};
    const LoginRaceTable tables[] = {LOGIN_RACE_TABLE_SLAYER, LOGIN_RACE_TABLE_VAMPIRE, LOGIN_RACE_TABLE_OUSTERS};
    const std::string slots[] = {"WRNG2",
                                 "slot2",
                                 "sLOT2",
                                 "SL0T2",
                                 " SLO2",
                                 "LOT2 ",
                                 std::string("SLO\0"
                                             "2",
                                             5),
                                 std::string("\0LOT2", 5)};

    for (int race = 0; race < 3; ++race) {
        for (const auto zone : {2101, 10001, 29999}) {
            for (const auto& slot : slots) {
                SCOPED_TRACE(::testing::Message() << race << "/" << zone << "/" << slot);
                FakeLoginCharacterRepository repository;
                FakeSelectPCTopology topology;
                repository.addSelectableCharacter(tables[race], "account", "Rowan", zone, slot, 30, 1);
                auto request = slayerRequest();
                request.pcType = types[race];

                const auto outcome = decideSelectPC(request, repository, topology);

                ASSERT_TRUE(outcome.isRejected());
                EXPECT_EQ(outcome.rejection(), SelectPCRejection::NoSlot);
                EXPECT_EQ(topology.isNonPKServerCalls, 1);
                EXPECT_TRUE(topology.zoneServerIDCalls.empty());
            }
        }
    }
}

TEST(DecideSelectPC, OnlyTheThreeSupportedSuffixBytesNameASlot) {
    for (int suffix = 0; suffix <= 255; ++suffix) {
        SCOPED_TRACE(suffix);
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        std::string slot = "SLOT";
        slot.push_back(static_cast<char>(suffix));
        repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, slot, 30, 1);

        const auto outcome = decideSelectPC(slayerRequest(), repository, topology);

        if (suffix >= '1' && suffix <= '3') {
            ASSERT_TRUE(outcome.isOk());
            EXPECT_EQ(outcome.events().slot, suffix - '0');
            EXPECT_EQ(topology.zoneServerIDCalls, (std::vector<ZoneID_t>{2101}));
        } else {
            ASSERT_TRUE(outcome.isRejected());
            EXPECT_EQ(outcome.rejection(), SelectPCRejection::NoSlot);
            EXPECT_TRUE(topology.zoneServerIDCalls.empty());
        }
        EXPECT_EQ(topology.isNonPKServerCalls, 1);
    }
}

// --- precedence -----------------------------------------------------------

TEST(DecideSelectPC, EarlierGatesWinOverAMalformedFiveByteSlot) {
    struct Gate {
        bool agreed;
        bool managingCharacters;
        bool hasCharacter;
        bool freePlayCap;
        SelectPCRejection rejection;
        int characterReads;
        int topologyReads;
    };
    const Gate gates[] = {{false, false, true, true, SelectPCRejection::DidNotAgree, 0, 0},
                          {true, false, true, true, SelectPCRejection::InvalidStatus, 0, 0},
                          {true, true, false, true, SelectPCRejection::NoSuchCharacter, 1, 0},
                          {true, true, true, true, SelectPCRejection::FreePlayLimit, 1, 0},
                          {true, true, true, false, SelectPCRejection::NonPKServerLimit, 1, 1}};
    for (const auto& gate : gates) {
        SCOPED_TRACE(static_cast<int>(gate.rejection));
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        topology.nonPKServer = true;
        if (gate.hasCharacter)
            repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT0", 90, 3);
        auto request = slayerRequest();
        request.agreedToTerms = gate.agreed;
        request.inCharacterManagement = gate.managingCharacters;
        request.checkFreePlayLimit = gate.freePlayCap;
        request.freePlaySlayerDomainSum = 80;

        const auto outcome = decideSelectPC(request, repository, topology);

        ASSERT_TRUE(outcome.isRejected());
        EXPECT_EQ(outcome.rejection(), gate.rejection);
        EXPECT_EQ(repository.loadCharacterForSelectCalls, gate.characterReads);
        EXPECT_EQ(topology.isNonPKServerCalls, gate.topologyReads);
        EXPECT_TRUE(topology.zoneServerIDCalls.empty());
    }
}

TEST(DecideSelectPC, TheTermsWinOverEveryOtherReason) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    topology.nonPKServer = true;

    SelectPCRequest request = slayerRequest();
    request.agreedToTerms = false;
    request.inCharacterManagement = false;

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::DidNotAgree, outcome.rejection());
}

TEST(DecideSelectPC, TheSessionStatusWinsOverAMissingCharacter) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;

    SelectPCRequest request = slayerRequest();
    request.inCharacterManagement = false;

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::InvalidStatus, outcome.rejection());
}

TEST(DecideSelectPC, TheFreePlayCapWinsOverTheNonPKRule) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    topology.nonPKServer = true;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", 90, 3);

    SelectPCRequest request = slayerRequest();
    request.checkFreePlayLimit = true;
    request.freePlaySlayerDomainSum = 40;

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::FreePlayLimit, outcome.rejection());
    EXPECT_EQ(0, topology.isNonPKServerCalls);
}

TEST(DecideSelectPC, TheNonPKRuleWinsOverABrokenSlotText) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    topology.nonPKServer = true;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "", 90, 3);

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::NonPKServerLimit, outcome.rejection());
}

TEST(DecideSelectPC, ABrokenSlotTextIsRefusedBeforeAnyRouting) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT", 30, 1);

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::NoSlot, outcome.rejection());
    EXPECT_TRUE(topology.zoneServerIDCalls.empty());
}

// --- the selected character -----------------------------------------------

TEST(DecideSelectPC, AnOrdinaryZoneIsRoutedThroughItsZoneGroupsServer) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    topology.serverID = 4;
    addRowan(repository);

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

    ASSERT_TRUE(outcome.isOk());
    const SelectedCharacter& selected = outcome.events();

    EXPECT_EQ(LOGIN_RACE_TABLE_SLAYER, selected.table);
    EXPECT_EQ(2101, selected.zoneID);
    EXPECT_EQ(4, selected.serverID);
    EXPECT_EQ(2, selected.slot);

    ASSERT_EQ(1u, topology.zoneServerIDCalls.size());
    EXPECT_EQ(2101, topology.zoneServerIDCalls[0]);
}

TEST(DecideSelectPC, TheSlotIsTheDigitOfTheSlotText) {
    const char* const slots[] = {"SLOT1", "SLOT2", "SLOT3"};

    for (int i = 0; i < 3; i++) {
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, slots[i], 30, 1);

        Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

        ASSERT_TRUE(outcome.isOk()) << slots[i];
        EXPECT_EQ(i + 1, outcome.events().slot) << slots[i];
    }
}

TEST(DecideSelectPC, CreationSlotNamesKeepOneBasedAccountValuesForEveryRaceAndRoutingPath) {
    const PCType types[] = {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS};
    const LoginRaceTable tables[] = {LOGIN_RACE_TABLE_SLAYER, LOGIN_RACE_TABLE_VAMPIRE, LOGIN_RACE_TABLE_OUSTERS};
    for (int race = 0; race < 3; ++race) {
        for (int slot = SLOT1; slot < SLOT_MAX; ++slot) {
            for (const auto zone : {2101, 20000}) {
                SCOPED_TRACE(::testing::Message() << race << "/" << slot << "/" << zone);
                FakeLoginCharacterRepository repository;
                FakeSelectPCTopology topology;
                repository.addSelectableCharacter(tables[race], "account", "Rowan", zone, Slot2String[slot], 30, 1);
                auto request = slayerRequest();
                request.pcType = types[race];

                const auto outcome = decideSelectPC(request, repository, topology);

                ASSERT_TRUE(outcome.isOk());
                EXPECT_EQ(outcome.events().table, tables[race]);
                EXPECT_EQ(outcome.events().slot, slot + 1);
                EXPECT_EQ(outcome.events().zoneID, zone);
                EXPECT_EQ(outcome.events().serverID, zone == 20000 ? 1 : FakeSelectPCTopology::kDefaultServerID);
                EXPECT_EQ(topology.zoneServerIDCalls.size(), zone == 20000 ? 0u : 1u);
            }
        }
    }
}

TEST(DecideSelectPC, AQuestZoneGoesToTheGroupsFirstServerWithoutALookup) {
    const ZoneID_t zoneIDs[] = {10001, 20000, 29999};

    for (unsigned int i = 0; i < sizeof(zoneIDs) / sizeof(zoneIDs[0]); i++) {
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        topology.serverID = 4;
        repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", zoneIDs[i], "SLOT2", 30, 1);

        Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

        ASSERT_TRUE(outcome.isOk()) << zoneIDs[i];
        EXPECT_EQ(1, outcome.events().serverID) << zoneIDs[i];
        EXPECT_TRUE(topology.zoneServerIDCalls.empty()) << zoneIDs[i];
    }
}

TEST(DecideSelectPC, TheQuestZoneRangeIsExclusiveAtBothEnds) {
    const ZoneID_t zoneIDs[] = {10000, 30000};

    for (unsigned int i = 0; i < sizeof(zoneIDs) / sizeof(zoneIDs[0]); i++) {
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        topology.serverID = 4;
        repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", zoneIDs[i], "SLOT2", 30, 1);

        Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

        ASSERT_TRUE(outcome.isOk()) << zoneIDs[i];
        EXPECT_EQ(4, outcome.events().serverID) << zoneIDs[i];
        ASSERT_EQ(1u, topology.zoneServerIDCalls.size()) << zoneIDs[i];
    }
}

TEST(DecideSelectPC, TheNonPKFlagIsReadEvenForACharacterInAQuestZone) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    topology.nonPKServer = true;
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 20000, "SLOT2", 81, 3);

    Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(slayerRequest(), repository, topology);

    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(SelectPCRejection::NonPKServerLimit, outcome.rejection());
    EXPECT_EQ(1, topology.isNonPKServerCalls);
}

TEST(DecideSelectPC, EachRaceKeepsItsOwnTableInThePayload) {
    const PCType types[] = {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS};
    const LoginRaceTable tables[] = {LOGIN_RACE_TABLE_SLAYER, LOGIN_RACE_TABLE_VAMPIRE, LOGIN_RACE_TABLE_OUSTERS};

    for (int i = 0; i < 3; i++) {
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        topology.serverID = 9;
        repository.addSelectableCharacter(tables[i], "account", "Rowan", 3001, "SLOT3", 12, 0);

        SelectPCRequest request = slayerRequest();
        request.pcType = types[i];

        Outcome<SelectedCharacter, SelectPCRejection> outcome = decideSelectPC(request, repository, topology);

        ASSERT_TRUE(outcome.isOk()) << i;
        EXPECT_EQ(tables[i], outcome.events().table);
        EXPECT_EQ(3001, outcome.events().zoneID);
        EXPECT_EQ(9, outcome.events().serverID);
        EXPECT_EQ(3, outcome.events().slot);
    }
}

TEST(DecideSelectPC, FailedDiagnosticsPreserveBothRefusalAndAcceptedRouting) {
    for (bool refused : {false, true}) {
        FakeLoginCharacterRepository repository;
        FakeSelectPCTopology topology;
        topology.nonPKServer = true;
        repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 2101, "SLOT2", refused ? 81 : 30,
                                          3);
        unsigned groupReports = 0;
        unsigned routeReports = 0;
        const SelectPCDiagnostics diagnostics{[&](WorldID_t world, ServerGroupID_t group) {
                                                  EXPECT_EQ(world, 1);
                                                  EXPECT_EQ(group, 2);
                                                  ++groupReports;
                                                  throw std::runtime_error("group report failed");
                                              },
                                              [&](WorldID_t world, ServerGroupID_t group, ServerID_t server) {
                                                  EXPECT_EQ(world, 1);
                                                  EXPECT_EQ(group, 2);
                                                  EXPECT_EQ(server, FakeSelectPCTopology::kDefaultServerID);
                                                  ++routeReports;
                                                  throw NoSuchElementException("route report failed");
                                              }};
        auto result = decideSelectPC(slayerRequest(), repository, topology, diagnostics);
        ASSERT_EQ(result.isRejected(), refused);
        if (refused)
            EXPECT_EQ(result.rejection(), SelectPCRejection::NonPKServerLimit);
        else
            EXPECT_EQ(result.events().serverID, FakeSelectPCTopology::kDefaultServerID);
        EXPECT_EQ(groupReports, 1u);
        EXPECT_EQ(routeReports, refused ? 0u : 1u);
    }
}

TEST(DecideSelectPC, DiagnosticsFollowOnlyTheExecutedNonPKAndOrdinaryRoutingPaths) {
    FakeLoginCharacterRepository repository;
    FakeSelectPCTopology topology;
    addRowan(repository);
    unsigned groupReports = 0;
    unsigned routeReports = 0;
    const SelectPCDiagnostics diagnostics{[&](WorldID_t, ServerGroupID_t) { ++groupReports; },
                                          [&](WorldID_t, ServerGroupID_t, ServerID_t) { ++routeReports; }};
    auto request = slayerRequest();
    request.agreedToTerms = false;
    EXPECT_TRUE(decideSelectPC(request, repository, topology, diagnostics).isRejected());
    EXPECT_EQ(groupReports, 0u);
    EXPECT_EQ(routeReports, 0u);
    request.agreedToTerms = true;
    EXPECT_TRUE(decideSelectPC(request, repository, topology, diagnostics).isOk());
    EXPECT_EQ(groupReports, 0u);
    EXPECT_EQ(routeReports, 1u);
    repository.addSelectableCharacter(LOGIN_RACE_TABLE_SLAYER, "account", "Rowan", 20000, "SLOT2", 30, 1);
    topology.nonPKServer = true;
    auto result = decideSelectPC(request, repository, topology, diagnostics);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.events().serverID, 1);
    EXPECT_EQ(groupReports, 1u);
    EXPECT_EQ(routeReports, 1u);
}

} // namespace
