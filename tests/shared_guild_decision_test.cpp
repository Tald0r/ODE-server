// The sharedserver's guild mutation decisions
// (src/server/sharedserver/GuildDecision.cpp): every branch of founding a
// guild, carrying its registration through, expelling a member, quitting,
// breaking the guild up and changing a rank, the precedence between the
// refusals, and the ordered trace each answer produces - the character-side
// writes, the roster and guild mutations and the packets, interleaved as the
// handlers perform them. The repository is a fake and the rest of the trace is
// recorded beside its calls, so no database and no guild tables are involved;
// the handlers themselves are not exercised here because they need the
// GuildManager, a socket and the StringPool.

#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "FakeSharedGuildRepository.h"
#include "GuildDecision.h"

namespace {

const GuildID_t kGuildID = 7;
const char* kMaster = "Ondra";
const char* kMember = "Aria";
const char* kSubmaster = "Kesh";
const char* kSender = "Bran";

// A race value outside Guild::GuildRace. Nothing is written to a character row
// for one.
const GuildRace_t kUnknownRace = 3;

// MIN_GUILDMEMBER_COUNT and the count a waiting guild has to pass, as
// Guild.h and GSAddGuildMemberHandler hold them.
const int kMinMemberCount = 3;
const int kActivationThreshold = 4;

// RETURN_SLAYER_MASTER_GOLD and RETURN_SLAYER_SUBMASTER_GOLD.
const Gold_t kMasterRefund = 9000000;
const Gold_t kSubmasterRefund = 0;

typedef std::vector<std::string> Trace;
typedef std::vector<SharedGuildRosterEntry> Roster;

//////////////////////////////////////////////////////////////////////////////
// The step runner, as the sharedserver's own performs the steps
//////////////////////////////////////////////////////////////////////////////

// The StringPool stands in as its id: what matters is which entry a step
// names, not the text it holds.
std::string messageText(int message) {
    return "STR" + std::to_string(message);
}

void runSteps(const std::vector<SharedGuildStep>& steps, FakeSharedGuildRepository& repository,
              const std::string& departureLabel) {
    for (const SharedGuildStep& step : steps) {
        switch (step.action) {
        case SharedGuildAction::LogGuildExit:
            repository.record("log GuildExit(" + departureLabel + "," + step.name + "," + step.sender + ")");
            break;
        case SharedGuildAction::LogGuildBroken:
            repository.record("log GuildBroken(" + departureLabel + "," + step.name + "," +
                              std::to_string(step.memberCount) + ")");
            break;
        case SharedGuildAction::StampRequestDateTime:
            repository.stampMemberRequestDateTime(step.name);
            break;
        case SharedGuildAction::SetCharacterGuildID:
            repository.setCharacterGuildID(step.race, step.characterGuildID, step.name);
            break;
        case SharedGuildAction::AddCharacterGold:
            repository.addCharacterGold(step.race, (int)step.gold, step.name);
            break;
        case SharedGuildAction::InsertMessage:
            repository.insertMessage(step.spelling, step.name, messageText(step.message));
            break;
        case SharedGuildAction::ExpireMember:
            repository.record("expire(" + step.name + ")");
            break;
        case SharedGuildAction::LeaveMember:
            repository.record("leave(" + step.name + ")");
            break;
        case SharedGuildAction::DropMember:
            repository.record("deleteMember(" + step.name + ")");
            break;
        case SharedGuildAction::FreeMember:
            repository.record("free(" + step.name + ")");
            break;
        case SharedGuildAction::ClearMembers:
            repository.record("clearMembers");
            break;
        case SharedGuildAction::ModifyMemberRank:
            repository.record("modifyMemberRank(" + step.name + "," + std::to_string((int)step.rank) + ")");
            break;
        case SharedGuildAction::SetGuildMaster:
            repository.record("setMaster(" + step.name + ")");
            break;
        case SharedGuildAction::SetGuildState:
            repository.record("setState(" + std::to_string((int)step.guildID) + "," + std::to_string((int)step.state) +
                              ")");
            break;
        case SharedGuildAction::DeleteGuild:
            repository.record("deleteGuild(" + std::to_string((int)step.guildID) + ")");
            break;
        case SharedGuildAction::SendModifyGuildOK:
            repository.record("send SGModifyGuildOK(" + std::to_string((int)step.guildID) + "," +
                              std::to_string((int)step.state) + ")");
            break;
        case SharedGuildAction::SendExpelGuildMemberOK:
            repository.record("send SGExpelGuildMemberOK(" + std::to_string((int)step.guildID) + "," + step.name + "," +
                              step.sender + ")");
            break;
        case SharedGuildAction::SendQuitGuildOK:
            repository.record("send SGQuitGuildOK(" + std::to_string((int)step.guildID) + "," + step.name + ")");
            break;
        case SharedGuildAction::SendDeleteGuildOK:
            repository.record("send SGDeleteGuildOK(" + std::to_string((int)step.guildID) + ")");
            break;
        case SharedGuildAction::SendModifyGuildMemberOK:
            repository.record("send SGModifyGuildMemberOK(" + std::to_string((int)step.guildID) + "," + step.name +
                              "," + std::to_string((int)step.rank) + "," + step.sender + ")");
            break;
        }
    }
}

// The trace an answer leaves behind.
Trace traceOf(const std::vector<SharedGuildStep>& steps, const std::string& departureLabel = "Quit") {
    FakeSharedGuildRepository repository;
    runSteps(steps, repository, departureLabel);
    return repository.calls;
}

//////////////////////////////////////////////////////////////////////////////
// Request builders
//////////////////////////////////////////////////////////////////////////////

AddGuildRequest addGuildRequest(GuildRace_t race) {
    AddGuildRequest request;
    request.guildRace = race;
    request.guildID = 41;
    request.maxSlayerZoneID = 100;
    request.maxVampireZoneID = 200;
    request.maxOustersZoneID = 300;
    return request;
}

GuildActivationRequest activationRequest(GuildRace_t race, GuildState_t state, int activeMemberCount,
                                         const Roster& roster) {
    GuildActivationRequest request;
    request.guildID = kGuildID;
    request.guildRace = race;
    request.guildState = state;
    request.activeMemberCount = activeMemberCount;
    request.activationThreshold = kActivationThreshold;
    request.roster = roster;
    return request;
}

ExpelGuildMemberRequest expelRequest(GuildRace_t race) {
    ExpelGuildMemberRequest request;
    request.guildID = kGuildID;
    request.name = kMember;
    request.sender = kSender;
    request.guildExists = true;
    request.memberExists = true;
    request.guildRace = race;
    return request;
}

QuitGuildRequest quitRequest(GuildRace_t race, GuildState_t state, GuildMemberRank_t rank) {
    QuitGuildRequest request;
    request.guildID = kGuildID;
    request.name = kMember;
    request.guildExists = true;
    request.memberExists = true;
    request.memberRank = rank;
    request.guildRace = race;
    request.guildState = state;
    request.masterRefund = kMasterRefund;
    request.submasterRefund = kSubmasterRefund;
    return request;
}

GuildBreakupRequest breakupRequest(GuildRace_t race, int activeMemberCount, const Roster& roster) {
    GuildBreakupRequest request;
    request.guildID = kGuildID;
    request.guildRace = race;
    request.guildState = kGuildStateActive;
    request.activeMemberCount = activeMemberCount;
    request.minMemberCount = kMinMemberCount;
    request.cause = kMember;
    request.oustersNoGuildID = kNoGuildIDOusters;
    request.roster = roster;
    return request;
}

ModifyGuildMemberRequest modifyRequest(GuildRace_t race, GuildMemberRank_t memberRank,
                                       GuildMemberRank_t requestedRank) {
    ModifyGuildMemberRequest request;
    request.guildID = kGuildID;
    request.name = kMember;
    request.sender = kMaster;
    request.requestedRank = requestedRank;
    request.guildExists = true;
    request.memberExists = true;
    request.memberRank = memberRank;
    request.guildMaster = kMaster;
    request.guildRace = race;
    return request;
}

//////////////////////////////////////////////////////////////////////////////
// The race tables
//////////////////////////////////////////////////////////////////////////////

TEST(SharedGuildRace, KnownRacesAreTheThreePlayableOnes) {
    EXPECT_TRUE(isKnownGuildRace(kGuildRaceSlayer));
    EXPECT_TRUE(isKnownGuildRace(kGuildRaceVampire));
    EXPECT_TRUE(isKnownGuildRace(kGuildRaceOusters));
    EXPECT_FALSE(isKnownGuildRace(kUnknownRace));
}

TEST(SharedGuildRace, EachRaceHasItsOwnNoGuildID) {
    EXPECT_EQ(noGuildIDFor(kGuildRaceSlayer), 99);
    EXPECT_EQ(noGuildIDFor(kGuildRaceVampire), 0);
    EXPECT_EQ(noGuildIDFor(kGuildRaceOusters), 66);
    EXPECT_EQ(noGuildIDFor(kUnknownRace), 0);
}

TEST(SharedGuildRace, OnlyTheSlayerCancellationUsesTheTeamWording) {
    EXPECT_EQ(guildCancelMessageFor(kGuildRaceSlayer), kMessageTeamCancel);
    EXPECT_EQ(guildCancelMessageFor(kGuildRaceVampire), kMessageClanCancel);
    EXPECT_EQ(guildCancelMessageFor(kGuildRaceOusters), kMessageClanCancel);
}

//////////////////////////////////////////////////////////////////////////////
// decideAddGuild
//////////////////////////////////////////////////////////////////////////////

TEST(DecideAddGuild, EachRaceTakesTheNextZoneIDOfItsOwn) {
    Outcome<AddGuildAllocation, SharedGuildRejection> slayer = decideAddGuild(addGuildRequest(kGuildRaceSlayer));
    ASSERT_TRUE(slayer.isOk());
    EXPECT_EQ(slayer.events().guildID, 41);
    EXPECT_EQ(slayer.events().zoneID, 100);

    Outcome<AddGuildAllocation, SharedGuildRejection> vampire = decideAddGuild(addGuildRequest(kGuildRaceVampire));
    ASSERT_TRUE(vampire.isOk());
    EXPECT_EQ(vampire.events().zoneID, 200);

    Outcome<AddGuildAllocation, SharedGuildRejection> ousters = decideAddGuild(addGuildRequest(kGuildRaceOusters));
    ASSERT_TRUE(ousters.isOk());
    EXPECT_EQ(ousters.events().zoneID, 300);
}

TEST(DecideAddGuild, ARaceWhoseLastZoneIDIsNextGetsNoGuild) {
    AddGuildRequest request = addGuildRequest(kGuildRaceVampire);
    request.maxVampireZoneID = std::numeric_limits<ZoneID_t>::max();
    Outcome<AddGuildAllocation, SharedGuildRejection> outcome = decideAddGuild(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::ZoneIDsExhausted);

    request.maxVampireZoneID = std::numeric_limits<ZoneID_t>::max() - 1;
    ASSERT_TRUE(decideAddGuild(request).isOk());
}

TEST(DecideAddGuild, AnUnknownRaceGetsNoGuild) {
    Outcome<AddGuildAllocation, SharedGuildRejection> outcome = decideAddGuild(addGuildRequest(kUnknownRace));
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::UnknownRace);
    EXPECT_TRUE(outcome.rejection().steps.empty());
}

//////////////////////////////////////////////////////////////////////////////
// decideGuildActivation
//////////////////////////////////////////////////////////////////////////////

TEST(DecideGuildActivation, AGuildThatIsNotWaitingDoesNotActivate) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideGuildActivation(activationRequest(kGuildRaceSlayer, kGuildStateActive, 9, roster));
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::GuildNotWaiting);
}

TEST(DecideGuildActivation, TheThresholdHasToBePassedNotReached) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster)};

    Outcome<SharedGuildEvents, SharedGuildRejection> atThreshold =
        decideGuildActivation(activationRequest(kGuildRaceSlayer, kGuildStateWait, kActivationThreshold, roster));
    ASSERT_TRUE(atThreshold.isRejected());
    EXPECT_EQ(atThreshold.rejection().reason, SharedGuildReason::NotEnoughMembers);

    Outcome<SharedGuildEvents, SharedGuildRejection> pastThreshold =
        decideGuildActivation(activationRequest(kGuildRaceSlayer, kGuildStateWait, kActivationThreshold + 1, roster));
    EXPECT_TRUE(pastThreshold.isOk());
}

TEST(DecideGuildActivation, ASlayerGuildStampsPointsAndTellsEveryMemberInOrder) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster),
                  SharedGuildRosterEntry(kMember, kGuildMemberRankNormal)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideGuildActivation(activationRequest(kGuildRaceSlayer, kGuildStateWait, 5, roster));
    ASSERT_TRUE(outcome.isOk());
    EXPECT_FALSE(outcome.events().checkBreakup);

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"stampMemberRequestDateTime(Ondra)", "setCharacterGuildID(0,7,Ondra)",
                     "insertMessage(1,Ondra,STR0)", "stampMemberRequestDateTime(Aria)", "setCharacterGuildID(0,7,Aria)",
                     "insertMessage(1,Aria,STR1)", "setState(7,0)", "send SGModifyGuildOK(7,0)"}));
}

TEST(DecideGuildActivation, AVampireGuildUsesTheClanWording) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster),
                  SharedGuildRosterEntry(kMember, kGuildMemberRankNormal)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideGuildActivation(activationRequest(kGuildRaceVampire, kGuildStateWait, 5, roster));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"stampMemberRequestDateTime(Ondra)", "setCharacterGuildID(1,7,Ondra)",
                     "insertMessage(1,Ondra,STR2)", "stampMemberRequestDateTime(Aria)", "setCharacterGuildID(1,7,Aria)",
                     "insertMessage(1,Aria,STR3)", "setState(7,0)", "send SGModifyGuildOK(7,0)"}));
}

TEST(DecideGuildActivation, AnOustersGuildUsesTheClanWordingToo) {
    Roster roster{SharedGuildRosterEntry(kMember, kGuildMemberRankNormal)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideGuildActivation(activationRequest(kGuildRaceOusters, kGuildStateWait, 5, roster));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"stampMemberRequestDateTime(Aria)", "setCharacterGuildID(2,7,Aria)", "insertMessage(1,Aria,STR3)",
                     "setState(7,0)", "send SGModifyGuildOK(7,0)"}));
}

TEST(DecideGuildActivation, AnUnknownRaceStampsAndNothingElse) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster),
                  SharedGuildRosterEntry(kMember, kGuildMemberRankNormal)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideGuildActivation(activationRequest(kUnknownRace, kGuildStateWait, 5, roster));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"stampMemberRequestDateTime(Ondra)", "stampMemberRequestDateTime(Aria)", "setState(7,0)",
                     "send SGModifyGuildOK(7,0)"}));
}

//////////////////////////////////////////////////////////////////////////////
// decideExpelGuildMember
//////////////////////////////////////////////////////////////////////////////

TEST(DecideExpelGuildMember, AMissingGuildIsRefusedBeforeTheMemberIsLookedFor) {
    ExpelGuildMemberRequest request = expelRequest(kGuildRaceSlayer);
    request.guildExists = false;
    request.memberExists = false;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideExpelGuildMember(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::GuildMissing);
    EXPECT_TRUE(outcome.rejection().steps.empty());
}

TEST(DecideExpelGuildMember, AMissingMemberIsRefused) {
    ExpelGuildMemberRequest request = expelRequest(kGuildRaceSlayer);
    request.memberExists = false;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideExpelGuildMember(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::MemberMissing);
    EXPECT_TRUE(outcome.rejection().steps.empty());
}

TEST(DecideExpelGuildMember, TheExpelIsLoggedBeforeAnythingIsWritten) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideExpelGuildMember(expelRequest(kGuildRaceSlayer));
    ASSERT_TRUE(outcome.isOk());
    EXPECT_TRUE(outcome.events().checkBreakup);

    EXPECT_EQ(traceOf(outcome.events().steps, "Expel"),
              (Trace{"log GuildExit(Expel,Aria,Bran)", "setCharacterGuildID(0,99,Aria)", "expire(Aria)",
                     "deleteMember(Aria)", "send SGExpelGuildMemberOK(7,Aria,Bran)"}));
}

TEST(DecideExpelGuildMember, EachRaceClearsItsOwnNoGuildID) {
    Outcome<SharedGuildEvents, SharedGuildRejection> vampire = decideExpelGuildMember(expelRequest(kGuildRaceVampire));
    ASSERT_TRUE(vampire.isOk());
    EXPECT_EQ(traceOf(vampire.events().steps, "Expel")[1], "setCharacterGuildID(1,0,Aria)");

    Outcome<SharedGuildEvents, SharedGuildRejection> ousters = decideExpelGuildMember(expelRequest(kGuildRaceOusters));
    ASSERT_TRUE(ousters.isOk());
    EXPECT_EQ(traceOf(ousters.events().steps, "Expel")[1], "setCharacterGuildID(2,66,Aria)");
}

TEST(DecideExpelGuildMember, AnUnknownRaceTouchesNoCharacterRow) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideExpelGuildMember(expelRequest(kUnknownRace));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps, "Expel"),
              (Trace{"log GuildExit(Expel,Aria,Bran)", "expire(Aria)", "deleteMember(Aria)",
                     "send SGExpelGuildMemberOK(7,Aria,Bran)"}));
}

//////////////////////////////////////////////////////////////////////////////
// decideGuildBreakup
//////////////////////////////////////////////////////////////////////////////

TEST(DecideGuildBreakup, OnlyAnActiveGuildFallsApart) {
    GuildBreakupRequest request = breakupRequest(kGuildRaceSlayer, 1, Roster{});
    request.guildState = kGuildStateWait;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideGuildBreakup(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::GuildNotActive);
}

TEST(DecideGuildBreakup, TheMinimumIsTheCountTheGuildMayKeep) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster)};

    Outcome<SharedGuildEvents, SharedGuildRejection> atMinimum =
        decideGuildBreakup(breakupRequest(kGuildRaceSlayer, kMinMemberCount, roster));
    ASSERT_TRUE(atMinimum.isRejected());
    EXPECT_EQ(atMinimum.rejection().reason, SharedGuildReason::EnoughMembers);

    Outcome<SharedGuildEvents, SharedGuildRejection> belowMinimum =
        decideGuildBreakup(breakupRequest(kGuildRaceSlayer, kMinMemberCount - 1, roster));
    EXPECT_TRUE(belowMinimum.isOk());
}

TEST(DecideGuildBreakup, AnExpelBreakupTellsNobodyAndClearsToTheRacesNoGuildID) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster),
                  SharedGuildRosterEntry(kSubmaster, kGuildMemberRankSubmaster)};

    GuildBreakupRequest request = breakupRequest(kGuildRaceOusters, 2, roster);
    request.notifyMembers = false;
    request.oustersNoGuildID = kNoGuildIDOusters;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideGuildBreakup(request);
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps, "Expel"),
              (Trace{"log GuildBroken(Expel,Aria,2)", "setCharacterGuildID(2,66,Ondra)", "expire(Ondra)", "free(Ondra)",
                     "setCharacterGuildID(2,66,Kesh)", "expire(Kesh)", "free(Kesh)", "clearMembers", "setState(7,3)",
                     "deleteGuild(7)", "send SGDeleteGuildOK(7)"}));
}

TEST(DecideGuildBreakup, AQuitBreakupTellsEveryMemberAndClearsOustersToZero) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster)};

    GuildBreakupRequest request = breakupRequest(kGuildRaceOusters, 2, roster);
    request.notifyMembers = true;
    request.oustersNoGuildID = 0;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideGuildBreakup(request);
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildBroken(Quit,Aria,2)", "setCharacterGuildID(2,0,Ondra)", "insertMessage(0,Ondra,STR7)",
                     "expire(Ondra)", "free(Ondra)", "clearMembers", "setState(7,3)", "deleteGuild(7)",
                     "send SGDeleteGuildOK(7)"}));
}

TEST(DecideGuildBreakup, ASlayerBreakupUsesTheTeamWording) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster)};

    GuildBreakupRequest request = breakupRequest(kGuildRaceSlayer, 2, roster);
    request.notifyMembers = true;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideGuildBreakup(request);
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps)[2], "insertMessage(0,Ondra,STR6)");
}

TEST(DecideGuildBreakup, AnUnknownRaceOnlyExpiresTheMembers) {
    Roster roster{SharedGuildRosterEntry(kMaster, kGuildMemberRankMaster)};

    GuildBreakupRequest request = breakupRequest(kUnknownRace, 2, roster);
    request.notifyMembers = true;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideGuildBreakup(request);
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildBroken(Quit,Aria,2)", "expire(Ondra)", "free(Ondra)", "clearMembers", "setState(7,3)",
                     "deleteGuild(7)", "send SGDeleteGuildOK(7)"}));
}

//////////////////////////////////////////////////////////////////////////////
// decideQuitGuild
//////////////////////////////////////////////////////////////////////////////

TEST(DecideQuitGuild, AMissingGuildIsRefusedBeforeTheMemberIsLookedFor) {
    QuitGuildRequest request = quitRequest(kGuildRaceSlayer, kGuildStateActive, kGuildMemberRankNormal);
    request.guildExists = false;
    request.memberExists = false;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideQuitGuild(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::GuildMissing);
    EXPECT_TRUE(outcome.rejection().steps.empty());
}

TEST(DecideQuitGuild, AMissingMemberIsRefusedBeforeAnythingIsLogged) {
    QuitGuildRequest request = quitRequest(kGuildRaceSlayer, kGuildStateActive, kGuildMemberRankNormal);
    request.memberExists = false;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideQuitGuild(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::MemberMissing);
    EXPECT_TRUE(outcome.rejection().steps.empty());
}

TEST(DecideQuitGuild, AnActiveGuildsMasterMayNotLeaveButTheQuitIsStillLogged) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideQuitGuild(quitRequest(kGuildRaceSlayer, kGuildStateActive, kGuildMemberRankMaster));
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::GuildMasterMayNotQuit);

    EXPECT_EQ(traceOf(outcome.rejection().steps), (Trace{"log GuildExit(Quit,Aria,)"}));
}

TEST(DecideQuitGuild, LeavingAnActiveGuildClearsTheRowAndTellsTheGameServers) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideQuitGuild(quitRequest(kGuildRaceVampire, kGuildStateActive, kGuildMemberRankNormal));
    ASSERT_TRUE(outcome.isOk());
    EXPECT_TRUE(outcome.events().checkBreakup);

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildExit(Quit,Aria,)", "setCharacterGuildID(1,0,Aria)", "leave(Aria)", "deleteMember(Aria)",
                     "send SGQuitGuildOK(7,Aria)"}));
}

TEST(DecideQuitGuild, AWaitingApplicantLeavesAnActiveGuildWithNoLogLine) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideQuitGuild(quitRequest(kGuildRaceSlayer, kGuildStateActive, kGuildMemberRankWait));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps), (Trace{"setCharacterGuildID(0,99,Aria)", "leave(Aria)",
                                                      "deleteMember(Aria)", "send SGQuitGuildOK(7,Aria)"}));
}

TEST(DecideQuitGuild, AWaitingGuildsMasterGivesTheRegistrationUpAndPaysTheFeesBack) {
    QuitGuildRequest request = quitRequest(kGuildRaceSlayer, kGuildStateWait, kGuildMemberRankMaster);
    request.roster = Roster{SharedGuildRosterEntry(kMember, kGuildMemberRankMaster),
                            SharedGuildRosterEntry(kSubmaster, kGuildMemberRankNormal)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideQuitGuild(request);
    ASSERT_TRUE(outcome.isOk());
    EXPECT_FALSE(outcome.events().checkBreakup);

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildExit(Quit,Aria,)", "insertMessage(0,Aria,STR8)", "addCharacterGold(0,9000000,Aria)",
                     "expire(Aria)", "free(Aria)", "expire(Kesh)", "free(Kesh)", "clearMembers", "setState(7,2)",
                     "deleteGuild(7)", "send SGDeleteGuildOK(7)"}));
}

TEST(DecideQuitGuild, ARefundOfNothingSendsNoMessageEither) {
    QuitGuildRequest request = quitRequest(kGuildRaceSlayer, kGuildStateWait, kGuildMemberRankMaster);
    request.submasterRefund = 0;
    request.roster = Roster{SharedGuildRosterEntry(kSubmaster, kGuildMemberRankSubmaster)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideQuitGuild(request);
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildExit(Quit,Aria,)", "expire(Kesh)", "free(Kesh)", "clearMembers", "setState(7,2)",
                     "deleteGuild(7)", "send SGDeleteGuildOK(7)"}));
}

TEST(DecideQuitGuild, ASubmasterWithAFeeToReturnIsPaidToo) {
    QuitGuildRequest request = quitRequest(kGuildRaceVampire, kGuildStateWait, kGuildMemberRankMaster);
    request.submasterRefund = 4500000;
    request.roster = Roster{SharedGuildRosterEntry(kSubmaster, kGuildMemberRankSubmaster)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideQuitGuild(request);
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildExit(Quit,Aria,)", "insertMessage(0,Kesh,STR9)", "addCharacterGold(1,4500000,Kesh)",
                     "expire(Kesh)", "free(Kesh)", "clearMembers", "setState(7,2)", "deleteGuild(7)",
                     "send SGDeleteGuildOK(7)"}));
}

TEST(DecideQuitGuild, AMissingCancellationTextStopsTheRefundBesideIt) {
    QuitGuildRequest request = quitRequest(kGuildRaceSlayer, kGuildStateWait, kGuildMemberRankMaster);
    request.cancelMessageEmpty = true;
    request.roster = Roster{SharedGuildRosterEntry(kMember, kGuildMemberRankMaster)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideQuitGuild(request);
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildExit(Quit,Aria,)", "expire(Aria)", "free(Aria)", "clearMembers", "setState(7,2)",
                     "deleteGuild(7)", "send SGDeleteGuildOK(7)"}));
}

TEST(DecideQuitGuild, AnUnknownRaceCancelsWithoutPayingAnybody) {
    QuitGuildRequest request = quitRequest(kUnknownRace, kGuildStateWait, kGuildMemberRankMaster);
    request.roster = Roster{SharedGuildRosterEntry(kMember, kGuildMemberRankMaster)};

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideQuitGuild(request);
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildExit(Quit,Aria,)", "expire(Aria)", "free(Aria)", "clearMembers", "setState(7,2)",
                     "deleteGuild(7)", "send SGDeleteGuildOK(7)"}));
}

TEST(DecideQuitGuild, ASubmasterTakesItsStartingMembershipBack) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideQuitGuild(quitRequest(kGuildRaceSlayer, kGuildStateWait, kGuildMemberRankSubmaster));
    ASSERT_TRUE(outcome.isOk());
    EXPECT_FALSE(outcome.events().checkBreakup);

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"log GuildExit(Quit,Aria,)", "expire(Aria)", "deleteMember(Aria)", "send SGQuitGuildOK(7,Aria)"}));
}

TEST(DecideQuitGuild, ANormalMemberOfAWaitingGuildIsLoggedAndNothingMore) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideQuitGuild(quitRequest(kGuildRaceSlayer, kGuildStateWait, kGuildMemberRankNormal));
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::NothingToDo);

    EXPECT_EQ(traceOf(outcome.rejection().steps), (Trace{"log GuildExit(Quit,Aria,)"}));
}

TEST(DecideQuitGuild, AWaitingApplicantOfAWaitingGuildLeavesNoTraceAtAll) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideQuitGuild(quitRequest(kGuildRaceSlayer, kGuildStateWait, kGuildMemberRankWait));
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::NothingToDo);
    EXPECT_TRUE(outcome.rejection().steps.empty());
}

TEST(DecideQuitGuild, AGuildAlreadyGoneDoesNothingButLog) {
    Outcome<SharedGuildEvents, SharedGuildRejection> broken =
        decideQuitGuild(quitRequest(kGuildRaceSlayer, kGuildStateBroken, kGuildMemberRankNormal));
    ASSERT_TRUE(broken.isRejected());
    EXPECT_EQ(broken.rejection().reason, SharedGuildReason::NothingToDo);
    EXPECT_EQ(traceOf(broken.rejection().steps), (Trace{"log GuildExit(Quit,Aria,)"}));

    Outcome<SharedGuildEvents, SharedGuildRejection> cancelled =
        decideQuitGuild(quitRequest(kGuildRaceSlayer, kGuildStateCancel, kGuildMemberRankMaster));
    ASSERT_TRUE(cancelled.isRejected());
    EXPECT_EQ(cancelled.rejection().reason, SharedGuildReason::NothingToDo);
}

//////////////////////////////////////////////////////////////////////////////
// decideModifyGuildMember
//////////////////////////////////////////////////////////////////////////////

TEST(DecideModifyGuildMember, AMissingGuildIsRefusedFirst) {
    ModifyGuildMemberRequest request = modifyRequest(kGuildRaceSlayer, kGuildMemberRankWait, kGuildMemberRankNormal);
    request.guildExists = false;
    request.memberExists = false;
    request.sender = kSender;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideModifyGuildMember(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::GuildMissing);
}

TEST(DecideModifyGuildMember, AMissingMemberIsRefusedBeforeTheSenderIsWeighed) {
    ModifyGuildMemberRequest request = modifyRequest(kGuildRaceSlayer, kGuildMemberRankWait, kGuildMemberRankNormal);
    request.memberExists = false;
    request.sender = kSender;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideModifyGuildMember(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::MemberMissing);
}

TEST(DecideModifyGuildMember, OnlyTheMasterMayChangeARank) {
    ModifyGuildMemberRequest request = modifyRequest(kGuildRaceSlayer, kGuildMemberRankWait, kGuildMemberRankNormal);
    request.sender = kSender;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideModifyGuildMember(request);
    ASSERT_TRUE(outcome.isRejected());
    EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::NotGuildMaster);
}

TEST(DecideModifyGuildMember, HandingTheGuildOverIsAskedForByAnybody) {
    ModifyGuildMemberRequest request = modifyRequest(kGuildRaceSlayer, kGuildMemberRankNormal, kGuildMemberRankMaster);
    request.sender = kSender;

    Outcome<SharedGuildEvents, SharedGuildRejection> outcome = decideModifyGuildMember(request);
    EXPECT_TRUE(outcome.isOk());
}

TEST(DecideModifyGuildMember, AnAcceptedApplicantIsPointedAtTheGuildBeforeItsRankMoves) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideModifyGuildMember(modifyRequest(kGuildRaceSlayer, kGuildMemberRankWait, kGuildMemberRankNormal));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"setCharacterGuildID(0,7,Aria)", "insertMessage(0,Aria,STR4)", "modifyMemberRank(Aria,0)",
                     "send SGModifyGuildMemberOK(7,Aria,0,Ondra)"}));
}

TEST(DecideModifyGuildMember, AVampireApplicantGetsTheClanWording) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideModifyGuildMember(modifyRequest(kGuildRaceVampire, kGuildMemberRankWait, kGuildMemberRankNormal));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps)[1], "insertMessage(0,Aria,STR5)");
}

TEST(DecideModifyGuildMember, AnUnknownRaceOnlyMovesTheRank) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideModifyGuildMember(modifyRequest(kUnknownRace, kGuildMemberRankWait, kGuildMemberRankNormal));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"modifyMemberRank(Aria,0)", "send SGModifyGuildMemberOK(7,Aria,0,Ondra)"}));
}

TEST(DecideModifyGuildMember, TheOldMasterDropsToTheRankTheNewOneHeld) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideModifyGuildMember(modifyRequest(kGuildRaceSlayer, kGuildMemberRankSubmaster, kGuildMemberRankMaster));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"modifyMemberRank(Ondra,2)", "modifyMemberRank(Aria,1)", "setMaster(Aria)",
                     "send SGModifyGuildMemberOK(7,Aria,1,Ondra)"}));
}

TEST(DecideModifyGuildMember, ANormalMemberBecomesASubmaster) {
    Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
        decideModifyGuildMember(modifyRequest(kGuildRaceSlayer, kGuildMemberRankNormal, kGuildMemberRankSubmaster));
    ASSERT_TRUE(outcome.isOk());

    EXPECT_EQ(traceOf(outcome.events().steps),
              (Trace{"modifyMemberRank(Aria,2)", "send SGModifyGuildMemberOK(7,Aria,2,Ondra)"}));
}

TEST(DecideModifyGuildMember, EveryOtherPairOfRanksIsRefused) {
    const GuildMemberRank_t pairs[][2] = {{kGuildMemberRankMaster, kGuildMemberRankSubmaster},
                                          {kGuildMemberRankWait, kGuildMemberRankSubmaster},
                                          {kGuildMemberRankSubmaster, kGuildMemberRankNormal},
                                          {kGuildMemberRankNormal, kGuildMemberRankNormal},
                                          {kGuildMemberRankMaster, kGuildMemberRankMaster}};

    for (const GuildMemberRank_t* pair : pairs) {
        Outcome<SharedGuildEvents, SharedGuildRejection> outcome =
            decideModifyGuildMember(modifyRequest(kGuildRaceSlayer, pair[0], pair[1]));
        ASSERT_TRUE(outcome.isRejected());
        EXPECT_EQ(outcome.rejection().reason, SharedGuildReason::RankChangeNotAllowed);
    }
}

} // namespace
