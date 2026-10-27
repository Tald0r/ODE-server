//////////////////////////////////////////////////////////////////////////////
// Filename    : GuildDecision.h
// Description : the rules the sharedserver applies to a guild mutation a game
//               server asks for - founding a guild, taking a member in,
//               expelling one, quitting, changing a rank - kept apart from the
//               handlers so they can be exercised with neither the guild
//               tables nor a database. Everything here needs plain values
//               only: the handler reads the guild, calls the decision,
//               performs the steps it answers in order and sends the packets
//               they name.
//////////////////////////////////////////////////////////////////////////////

#ifndef __GUILD_DECISION_H__
#define __GUILD_DECISION_H__

#include <string>
#include <vector>

#include "Outcome.h"
#include "Types.h"
#include "repository/SharedGuildRepository.h"

//////////////////////////////////////////////////////////////////////////////
// Vocabulary
//////////////////////////////////////////////////////////////////////////////

// Guild::GuildRace. The handlers static_assert these against the enumerators.
const GuildRace_t kGuildRaceSlayer = 0;
const GuildRace_t kGuildRaceVampire = 1;
const GuildRace_t kGuildRaceOusters = 2;

// GuildMember::GuildRank.
const GuildMemberRank_t kGuildMemberRankNormal = 0;
const GuildMemberRank_t kGuildMemberRankMaster = 1;
const GuildMemberRank_t kGuildMemberRankSubmaster = 2;
const GuildMemberRank_t kGuildMemberRankWait = 3;

// Guild::GuildState.
const GuildState_t kGuildStateActive = 0;
const GuildState_t kGuildStateWait = 1;
const GuildState_t kGuildStateCancel = 2;
const GuildState_t kGuildStateBroken = 3;

// StringPool's StringID, the text a message step carries.
const int kMessageNone = -1;
const int kMessageTeamRegistrationAccept = 0;
const int kMessageTeamRegistrationAccept2 = 1;
const int kMessageClanRegistrationAccept = 2;
const int kMessageClanRegistrationAccept2 = 3;
const int kMessageTeamJoinAccept = 4;
const int kMessageClanJoinAccept = 5;
const int kMessageTeamBroken = 6;
const int kMessageClanBroken = 7;
const int kMessageTeamCancel = 8;
const int kMessageClanCancel = 9;

// The value the character row's GuildID column carries while the character
// belongs to no guild. One per race, as the race tables were seeded.
const int kNoGuildIDSlayer = 99;
const int kNoGuildIDVampire = 0;
const int kNoGuildIDOusters = 66;

// Is this one of the three races the race tables cover? A request naming
// anything else writes no character row at all.
bool isKnownGuildRace(GuildRace_t race);

// The no-guild id of one race, zero for any other race.
int noGuildIDFor(GuildRace_t race);

// The StringPool id of the text every member is sent when a guild gives its
// registration up. The caller resolves it to ask whether the pool holds one.
int guildCancelMessageFor(GuildRace_t race);

//////////////////////////////////////////////////////////////////////////////
// The steps a decision answers
//////////////////////////////////////////////////////////////////////////////

// One thing the caller performs, in the order the steps list them.
enum class SharedGuildAction {
    // filelog GuildExit.log: name left the guild, sender expelled it where
    // there is one.
    LogGuildExit,
    // filelog GuildBroken.log: the guild fell apart with memberCount members
    // left, over name leaving.
    LogGuildBroken,

    // --- the character side, through SharedGuildRepository ------------------
    // stampMemberRequestDateTime(name).
    StampRequestDateTime,
    // setCharacterGuildID(race, characterGuildID, name).
    SetCharacterGuildID,
    // addCharacterGold(race, gold, name).
    AddCharacterGold,
    // insertMessage(spelling, name, the StringPool text of message).
    InsertMessage,

    // --- the roster ---------------------------------------------------------
    // GuildMember::expire - the row keeps the guild id and is stamped with
    // today.
    ExpireMember,
    // GuildMember::leave - the same, under the rank a voluntary quit leaves.
    LeaveMember,
    // Guild::deleteMember(name) - drops the member from the guild's map and
    // frees it.
    DropMember,
    // Free a member the breakup walk already expired, without touching the
    // map it is walking.
    FreeMember,
    // Empty the map the breakup walk left holding freed pointers.
    ClearMembers,
    // Guild::modifyMemberRank(name, rank).
    ModifyMemberRank,
    // Guild::setMaster(name) and the Master column beside it.
    SetGuildMaster,

    // --- the guild ----------------------------------------------------------
    // Guild::setState(state) and Guild::save().
    SetGuildState,
    // Free the guild object and drop it from the GuildManager. Nothing after
    // this may read the guild.
    DeleteGuild,

    // --- what goes back to the game servers ----------------------------------
    // SGModifyGuildOK(guildID, state).
    SendModifyGuildOK,
    // SGExpelGuildMemberOK(guildID, name, sender).
    SendExpelGuildMemberOK,
    // SGQuitGuildOK(guildID, name).
    SendQuitGuildOK,
    // SGDeleteGuildOK(guildID).
    SendDeleteGuildOK,
    // SGModifyGuildMemberOK(guildID, name, rank, sender).
    SendModifyGuildMemberOK
};

struct SharedGuildStep {
    SharedGuildAction action = SharedGuildAction::LogGuildExit;
    // The character the step is about.
    std::string name;
    // Whoever asked, echoed by the expel and rank-change packets.
    std::string sender;
    GuildID_t guildID = 0;
    // Guild::GuildRace, which chooses the race table.
    GuildRace_t race = 0;
    // What the character row's GuildID column becomes: the guild's own id on
    // joining, the race's no-guild id on leaving.
    int characterGuildID = 0;
    GuildMemberRank_t rank = 0;
    GuildState_t state = 0;
    Gold_t gold = 0;
    SharedMessageSpelling spelling = SHARED_MESSAGE_SQL_COMPACT;
    // A StringPool id, kMessageNone where the step carries no text.
    int message = kMessageNone;
    // What GuildBroken.log records.
    int memberCount = 0;
};

struct SharedGuildEvents {
    std::vector<SharedGuildStep> steps;
    // True where the guild survives the steps and may now be short of
    // members: the caller re-reads its roster and count and asks
    // decideGuildBreakup.
    bool checkBreakup = false;
};

//////////////////////////////////////////////////////////////////////////////
// Refusals
//////////////////////////////////////////////////////////////////////////////

// Why the sharedserver did not do what was asked. Every one of them is
// answered with silence: the game server that asked gets no packet back.
enum class SharedGuildReason {
    // The request names no guild race, so there is no zone id to hand out.
    UnknownRace,
    // The race's next guild zone id is the last one, and its counter cannot
    // move past it.
    ZoneIDsExhausted,
    // The id names no loaded guild.
    GuildMissing,
    // The name has no row in that guild's roster.
    MemberMissing,
    // The sender is not the guild master and is not handing the guild over.
    NotGuildMaster,
    // The rank the request asks for is not one the current rank may move to.
    RankChangeNotAllowed,
    // A guild master cannot walk out of an active guild; it hands the guild
    // over or breaks it up.
    GuildMasterMayNotQuit,
    // The member's rank has no quit path in the guild's state.
    NothingToDo,
    // The guild is not waiting for its registration, so nothing activates it.
    GuildNotWaiting,
    // The guild has not passed the member count its registration needs.
    NotEnoughMembers,
    // Only an active guild falls apart; one still waiting or already gone
    // does not.
    GuildNotActive,
    // The guild still has the members it needs to stand.
    EnoughMembers
};

struct SharedGuildRejection {
    explicit SharedGuildRejection(SharedGuildReason rejectReason) : reason(rejectReason) {}

    SharedGuildReason reason;
    // What the request still leaves behind on its way out. Only the refused
    // quit of a guild master carries anything: its quit log is written before
    // the rank is looked at.
    std::vector<SharedGuildStep> steps;
};

//////////////////////////////////////////////////////////////////////////////
// One member of a guild, as the decisions read the roster
//////////////////////////////////////////////////////////////////////////////

struct SharedGuildRosterEntry {
    SharedGuildRosterEntry(const std::string& memberName, GuildMemberRank_t memberRank)
        : name(memberName), rank(memberRank) {}

    std::string name;
    GuildMemberRank_t rank;
};

//////////////////////////////////////////////////////////////////////////////
// Founding a guild
//////////////////////////////////////////////////////////////////////////////

// What GSAddGuild asks. The guild id is the one after the highest claimed;
// the caller claims it only for an accepted founding.
struct AddGuildRequest {
    GuildRace_t guildRace = 0;
    GuildID_t guildID = 0;
    // Each race's guild zone counter, which the new guild takes as its zone id.
    ZoneID_t maxSlayerZoneID = 0;
    ZoneID_t maxVampireZoneID = 0;
    ZoneID_t maxOustersZoneID = 0;
};

struct AddGuildAllocation {
    GuildID_t guildID = 0;
    // The zone id the new guild takes; the race's counter moves past it.
    ZoneID_t zoneID = 0;
};

// What does the new guild get?
//
// Refused when the race is none of the three, or when the race's next zone id
// is the last one; either way the request is answered with nothing.
[[nodiscard]] Outcome<AddGuildAllocation, SharedGuildRejection> decideAddGuild(const AddGuildRequest& request);

//////////////////////////////////////////////////////////////////////////////
// A guild that has gathered enough members
//////////////////////////////////////////////////////////////////////////////

// What GSAddGuildMember asks once the new member is in the guild. The member
// is written and announced whatever the answer here is.
struct GuildActivationRequest {
    GuildID_t guildID = 0;
    GuildRace_t guildRace = 0;
    GuildState_t guildState = 0;
    // The guild's active member count with the new member counted.
    int activeMemberCount = 0;
    // The count the guild has to pass - not merely reach - to become active.
    int activationThreshold = 0;
    // Every member, in the order the caller walked the roster.
    std::vector<SharedGuildRosterEntry> roster;
};

// Does this guild's registration go through?
//
// Only a guild still waiting for it, and only once it holds more than the
// threshold in active members. Then every member's character row is pointed
// at the guild and told so, the guild turns active and the game servers are
// sent the new state.
[[nodiscard]] Outcome<SharedGuildEvents, SharedGuildRejection>
decideGuildActivation(const GuildActivationRequest& request);

//////////////////////////////////////////////////////////////////////////////
// Expelling a member
//////////////////////////////////////////////////////////////////////////////

struct ExpelGuildMemberRequest {
    GuildID_t guildID = 0;
    std::string name;
    std::string sender;
    bool guildExists = false;
    bool memberExists = false;
    GuildRace_t guildRace = 0;
};

// What does this expulsion do?
//
// Refused, in this order, when the id names no guild and when the name has no
// row in it. Otherwise the quit is logged, the character row loses its guild
// id, the roster row is expired and dropped, and the game servers are told.
// The guild may now be short of members, so the caller asks
// decideGuildBreakup next.
[[nodiscard]] Outcome<SharedGuildEvents, SharedGuildRejection>
decideExpelGuildMember(const ExpelGuildMemberRequest& request);

//////////////////////////////////////////////////////////////////////////////
// Quitting
//////////////////////////////////////////////////////////////////////////////

struct QuitGuildRequest {
    GuildID_t guildID = 0;
    std::string name;
    bool guildExists = false;
    bool memberExists = false;
    GuildMemberRank_t memberRank = 0;
    GuildRace_t guildRace = 0;
    GuildState_t guildState = 0;
    // The registration fee handed back when a waiting guild's master gives
    // up. Both figures are the Slayer ones whatever the guild's race, as they
    // always have been, and a zero refund stops the message beside it.
    Gold_t masterRefund = 0;
    Gold_t submasterRefund = 0;
    // The StringPool holds no text for the cancellation message; that too
    // stops both the message and the refund.
    bool cancelMessageEmpty = false;
    // Every member, in the order the caller walked the roster. Read only for
    // the cancellation.
    std::vector<SharedGuildRosterEntry> roster;
};

// What does this quit do?
//
// Refused, in this order, when the id names no guild and when the name has no
// row in it. A normal, master or submaster quit is logged whatever follows.
//
// Leaving an active guild takes the character row's guild id away, marks the
// roster row left and tells the game servers; the guild may now be short of
// members, so the caller asks decideGuildBreakup next. An active guild's
// master may not leave at all.
//
// Leaving a guild still waiting for its registration is the master giving up:
// every member is expired, the master and the submaster get their fee back
// with a message, and the guild is cancelled and deleted. A submaster simply
// takes its starting membership back. Any other rank does nothing.
[[nodiscard]] Outcome<SharedGuildEvents, SharedGuildRejection> decideQuitGuild(const QuitGuildRequest& request);

//////////////////////////////////////////////////////////////////////////////
// A guild that has fallen below its member count
//////////////////////////////////////////////////////////////////////////////

struct GuildBreakupRequest {
    GuildID_t guildID = 0;
    GuildRace_t guildRace = 0;
    GuildState_t guildState = 0;
    // The guild's active member count with the departure already taken off.
    int activeMemberCount = 0;
    // MIN_GUILDMEMBER_COUNT: below this an active guild falls apart.
    int minMemberCount = 0;
    // Whoever left, as GuildBroken.log records it.
    std::string cause;
    // The Ousters character row is cleared to 66 after an expulsion and to 0
    // after a quit. The other two races clear to their no-guild id either way.
    int oustersNoGuildID = 0;
    // A quit tells every remaining member the guild is gone; an expulsion
    // tells none of them.
    bool notifyMembers = false;
    // Every member left, in the order the caller walked the roster.
    std::vector<SharedGuildRosterEntry> roster;
};

// Does the guild fall apart?
//
// Only an active one, and only once it holds fewer than the minimum in active
// members. Then every member left loses its guild id, is told where the caller
// asks for it, and is expired and freed; the guild is marked broken, saved,
// freed and dropped, and the game servers are told to delete it.
[[nodiscard]] Outcome<SharedGuildEvents, SharedGuildRejection> decideGuildBreakup(const GuildBreakupRequest& request);

//////////////////////////////////////////////////////////////////////////////
// Changing a member's rank
//////////////////////////////////////////////////////////////////////////////

struct ModifyGuildMemberRequest {
    GuildID_t guildID = 0;
    std::string name;
    std::string sender;
    GuildMemberRank_t requestedRank = 0;
    bool guildExists = false;
    bool memberExists = false;
    GuildMemberRank_t memberRank = 0;
    std::string guildMaster;
    GuildRace_t guildRace = 0;
};

// What does this rank change do?
//
// Refused, in this order, when the id names no guild, when the name has no row
// in it, and when the sender is not the guild master - unless the request
// hands the guild over, which anyone may ask for.
//
// Three moves are allowed: a waiting applicant becomes a normal member, which
// also points its character row at the guild and tells it; any member below
// master becomes the master, which drops the old master to the promoted
// member's rank and writes the new name to the guild; and a normal member
// becomes a submaster. Every other pair of ranks is refused. The game servers
// are told the member's new rank.
[[nodiscard]] Outcome<SharedGuildEvents, SharedGuildRejection>
decideModifyGuildMember(const ModifyGuildMemberRequest& request);

#endif // __GUILD_DECISION_H__
