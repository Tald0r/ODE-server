//////////////////////////////////////////////////////////////////////////////
// Filename    : GuildDecision.cpp
// Description : the rules the sharedserver applies to a guild mutation a game
//               server asks for.
//////////////////////////////////////////////////////////////////////////////

#include "GuildDecision.h"

#include <limits>

namespace {

typedef Outcome<SharedGuildEvents, SharedGuildRejection> EventsResult;

// Is a rank one of the three the guild counts as active and logs a departure
// for?
bool isActiveRank(GuildMemberRank_t rank) {
    return rank == kGuildMemberRankNormal || rank == kGuildMemberRankMaster || rank == kGuildMemberRankSubmaster;
}

// The text a member is sent when the guild's registration goes through. The
// master is addressed differently from everybody else.
int registrationMessageFor(GuildRace_t race, GuildMemberRank_t rank) {
    if (race == kGuildRaceSlayer)
        return rank == kGuildMemberRankMaster ? kMessageTeamRegistrationAccept : kMessageTeamRegistrationAccept2;

    return rank == kGuildMemberRankMaster ? kMessageClanRegistrationAccept : kMessageClanRegistrationAccept2;
}

// The text a member is sent when its application is accepted.
int joinAcceptMessageFor(GuildRace_t race) {
    return race == kGuildRaceSlayer ? kMessageTeamJoinAccept : kMessageClanJoinAccept;
}

// The text every member is sent when the guild falls apart.
int brokenMessageFor(GuildRace_t race) {
    return race == kGuildRaceSlayer ? kMessageTeamBroken : kMessageClanBroken;
}

// The quit log a departure writes, for the ranks that write one.
void appendQuitLog(std::vector<SharedGuildStep>& steps, const std::string& name, const std::string& sender) {
    steps.push_back({.action = SharedGuildAction::LogGuildExit, .name = name, .sender = sender});
}

} // namespace

bool isKnownGuildRace(GuildRace_t race) {
    return race == kGuildRaceSlayer || race == kGuildRaceVampire || race == kGuildRaceOusters;
}

int noGuildIDFor(GuildRace_t race) {
    switch (race) {
    case kGuildRaceSlayer:
        return kNoGuildIDSlayer;
    case kGuildRaceVampire:
        return kNoGuildIDVampire;
    case kGuildRaceOusters:
        return kNoGuildIDOusters;
    default:
        return 0;
    }
}

int guildCancelMessageFor(GuildRace_t race) {
    return race == kGuildRaceSlayer ? kMessageTeamCancel : kMessageClanCancel;
}

Outcome<AddGuildAllocation, SharedGuildRejection> decideAddGuild(const AddGuildRequest& request) {
    typedef Outcome<AddGuildAllocation, SharedGuildRejection> Result;

    AddGuildAllocation allocation;
    allocation.guildID = request.guildID;

    switch (request.guildRace) {
    case kGuildRaceSlayer:
        allocation.zoneID = request.maxSlayerZoneID;
        break;
    case kGuildRaceVampire:
        allocation.zoneID = request.maxVampireZoneID;
        break;
    case kGuildRaceOusters:
        allocation.zoneID = request.maxOustersZoneID;
        break;
    default:
        return Result::Rejected(SharedGuildRejection(SharedGuildReason::UnknownRace));
    }

    if (allocation.zoneID == std::numeric_limits<ZoneID_t>::max())
        return Result::Rejected(SharedGuildRejection(SharedGuildReason::ZoneIDsExhausted));

    return Result::Ok(allocation);
}

Outcome<SharedGuildEvents, SharedGuildRejection> decideGuildActivation(const GuildActivationRequest& request) {
    if (request.guildState != kGuildStateWait)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::GuildNotWaiting));

    if (request.activeMemberCount <= request.activationThreshold)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::NotEnoughMembers));

    SharedGuildEvents events;

    for (const SharedGuildRosterEntry& member : request.roster) {
        // The request stamp is refreshed for every member, whatever the race
        // is: an unknown race stops the two writes below it, not this one.
        events.steps.push_back({.action = SharedGuildAction::StampRequestDateTime, .name = member.name});

        if (!isKnownGuildRace(request.guildRace))
            continue;

        events.steps.push_back({.action = SharedGuildAction::SetCharacterGuildID,
                                .name = member.name,
                                .race = request.guildRace,
                                .characterGuildID = request.guildID});
        events.steps.push_back({.action = SharedGuildAction::InsertMessage,
                                .name = member.name,
                                .spelling = SHARED_MESSAGE_SQL_SPACED,
                                .message = registrationMessageFor(request.guildRace, member.rank)});
    }

    events.steps.push_back(
        {.action = SharedGuildAction::SetGuildState, .guildID = request.guildID, .state = kGuildStateActive});
    events.steps.push_back(
        {.action = SharedGuildAction::SendModifyGuildOK, .guildID = request.guildID, .state = kGuildStateActive});

    return EventsResult::Ok(events);
}

Outcome<SharedGuildEvents, SharedGuildRejection> decideExpelGuildMember(const ExpelGuildMemberRequest& request) {
    if (!request.guildExists)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::GuildMissing));

    if (!request.memberExists)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::MemberMissing));

    SharedGuildEvents events;
    appendQuitLog(events.steps, request.name, request.sender);

    if (isKnownGuildRace(request.guildRace))
        events.steps.push_back({.action = SharedGuildAction::SetCharacterGuildID,
                                .name = request.name,
                                .race = request.guildRace,
                                .characterGuildID = noGuildIDFor(request.guildRace)});

    events.steps.push_back({.action = SharedGuildAction::ExpireMember, .name = request.name});
    events.steps.push_back({.action = SharedGuildAction::DropMember, .name = request.name});
    events.steps.push_back({.action = SharedGuildAction::SendExpelGuildMemberOK,
                            .name = request.name,
                            .sender = request.sender,
                            .guildID = request.guildID});

    events.checkBreakup = true;
    return EventsResult::Ok(events);
}

Outcome<SharedGuildEvents, SharedGuildRejection> decideQuitGuild(const QuitGuildRequest& request) {
    if (!request.guildExists)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::GuildMissing));

    if (!request.memberExists)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::MemberMissing));

    SharedGuildEvents events;

    // The departure is logged before the guild's state and the member's rank
    // decide whether anything else happens, so a refusal below still leaves
    // the line behind.
    if (isActiveRank(request.memberRank))
        appendQuitLog(events.steps, request.name, std::string());

    if (request.guildState == kGuildStateActive) {
        if (request.memberRank == kGuildMemberRankMaster) {
            SharedGuildRejection rejection(SharedGuildReason::GuildMasterMayNotQuit);
            rejection.steps = events.steps;
            return EventsResult::Rejected(rejection);
        }

        if (isKnownGuildRace(request.guildRace))
            events.steps.push_back({.action = SharedGuildAction::SetCharacterGuildID,
                                    .name = request.name,
                                    .race = request.guildRace,
                                    .characterGuildID = noGuildIDFor(request.guildRace)});

        events.steps.push_back({.action = SharedGuildAction::LeaveMember, .name = request.name});
        events.steps.push_back({.action = SharedGuildAction::DropMember, .name = request.name});
        events.steps.push_back(
            {.action = SharedGuildAction::SendQuitGuildOK, .name = request.name, .guildID = request.guildID});

        events.checkBreakup = true;
        return EventsResult::Ok(events);
    }

    if (request.guildState == kGuildStateWait) {
        if (request.memberRank == kGuildMemberRankMaster) {
            for (const SharedGuildRosterEntry& member : request.roster) {
                Gold_t refund = 0;
                if (member.rank == kGuildMemberRankMaster)
                    refund = request.masterRefund;
                else if (member.rank == kGuildMemberRankSubmaster)
                    refund = request.submasterRefund;

                // Only a member of a known race with a refund worth paying is
                // told anything; the message and the gold travel together.
                if (isKnownGuildRace(request.guildRace) && !request.cancelMessageEmpty && refund != 0) {
                    events.steps.push_back({.action = SharedGuildAction::InsertMessage,
                                            .name = member.name,
                                            .spelling = SHARED_MESSAGE_SQL_COMPACT,
                                            .message = guildCancelMessageFor(request.guildRace)});
                    events.steps.push_back({.action = SharedGuildAction::AddCharacterGold,
                                            .name = member.name,
                                            .race = request.guildRace,
                                            .gold = refund});
                }

                events.steps.push_back({.action = SharedGuildAction::ExpireMember, .name = member.name});
                events.steps.push_back({.action = SharedGuildAction::FreeMember, .name = member.name});
            }

            events.steps.push_back({.action = SharedGuildAction::ClearMembers});
            events.steps.push_back(
                {.action = SharedGuildAction::SetGuildState, .guildID = request.guildID, .state = kGuildStateCancel});
            events.steps.push_back({.action = SharedGuildAction::DeleteGuild, .guildID = request.guildID});
            events.steps.push_back({.action = SharedGuildAction::SendDeleteGuildOK, .guildID = request.guildID});

            return EventsResult::Ok(events);
        }

        if (request.memberRank == kGuildMemberRankSubmaster) {
            events.steps.push_back({.action = SharedGuildAction::ExpireMember, .name = request.name});
            events.steps.push_back({.action = SharedGuildAction::DropMember, .name = request.name});
            events.steps.push_back(
                {.action = SharedGuildAction::SendQuitGuildOK, .name = request.name, .guildID = request.guildID});

            return EventsResult::Ok(events);
        }
    }

    SharedGuildRejection rejection(SharedGuildReason::NothingToDo);
    rejection.steps = events.steps;
    return EventsResult::Rejected(rejection);
}

Outcome<SharedGuildEvents, SharedGuildRejection> decideGuildBreakup(const GuildBreakupRequest& request) {
    if (request.guildState != kGuildStateActive)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::GuildNotActive));

    if (request.activeMemberCount >= request.minMemberCount)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::EnoughMembers));

    SharedGuildEvents events;
    events.steps.push_back(
        {.action = SharedGuildAction::LogGuildBroken, .name = request.cause, .memberCount = request.activeMemberCount});

    for (const SharedGuildRosterEntry& member : request.roster) {
        if (isKnownGuildRace(request.guildRace)) {
            const int noGuildID =
                request.guildRace == kGuildRaceOusters ? request.oustersNoGuildID : noGuildIDFor(request.guildRace);

            events.steps.push_back({.action = SharedGuildAction::SetCharacterGuildID,
                                    .name = member.name,
                                    .race = request.guildRace,
                                    .characterGuildID = noGuildID});

            if (request.notifyMembers)
                events.steps.push_back({.action = SharedGuildAction::InsertMessage,
                                        .name = member.name,
                                        .spelling = SHARED_MESSAGE_SQL_COMPACT,
                                        .message = brokenMessageFor(request.guildRace)});
        }

        events.steps.push_back({.action = SharedGuildAction::ExpireMember, .name = member.name});
        events.steps.push_back({.action = SharedGuildAction::FreeMember, .name = member.name});
    }

    events.steps.push_back({.action = SharedGuildAction::ClearMembers});
    events.steps.push_back(
        {.action = SharedGuildAction::SetGuildState, .guildID = request.guildID, .state = kGuildStateBroken});
    events.steps.push_back({.action = SharedGuildAction::DeleteGuild, .guildID = request.guildID});
    events.steps.push_back({.action = SharedGuildAction::SendDeleteGuildOK, .guildID = request.guildID});

    return EventsResult::Ok(events);
}

Outcome<SharedGuildEvents, SharedGuildRejection> decideModifyGuildMember(const ModifyGuildMemberRequest& request) {
    if (!request.guildExists)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::GuildMissing));

    if (!request.memberExists)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::MemberMissing));

    // Handing the guild over is the one change the guild master does not have
    // to ask for.
    if (request.guildMaster != request.sender && request.requestedRank != kGuildMemberRankMaster)
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::NotGuildMaster));

    SharedGuildEvents events;

    if (request.memberRank == kGuildMemberRankWait && request.requestedRank == kGuildMemberRankNormal) {
        if (isKnownGuildRace(request.guildRace)) {
            events.steps.push_back({.action = SharedGuildAction::SetCharacterGuildID,
                                    .name = request.name,
                                    .race = request.guildRace,
                                    .characterGuildID = request.guildID});
            events.steps.push_back({.action = SharedGuildAction::InsertMessage,
                                    .name = request.name,
                                    .spelling = SHARED_MESSAGE_SQL_COMPACT,
                                    .message = joinAcceptMessageFor(request.guildRace)});
        }

        events.steps.push_back(
            {.action = SharedGuildAction::ModifyMemberRank, .name = request.name, .rank = request.requestedRank});
    } else if (request.memberRank != kGuildMemberRankMaster && request.requestedRank == kGuildMemberRankMaster) {
        // The old master drops to the rank the new one held.
        events.steps.push_back(
            {.action = SharedGuildAction::ModifyMemberRank, .name = request.guildMaster, .rank = request.memberRank});
        events.steps.push_back(
            {.action = SharedGuildAction::ModifyMemberRank, .name = request.name, .rank = request.requestedRank});
        events.steps.push_back({.action = SharedGuildAction::SetGuildMaster, .name = request.name});
    } else if (request.memberRank == kGuildMemberRankNormal && request.requestedRank == kGuildMemberRankSubmaster) {
        events.steps.push_back(
            {.action = SharedGuildAction::ModifyMemberRank, .name = request.name, .rank = request.requestedRank});
    } else {
        return EventsResult::Rejected(SharedGuildRejection(SharedGuildReason::RankChangeNotAllowed));
    }

    // The rank the game servers are told is the one the member now holds,
    // which every branch above has just written.
    events.steps.push_back({.action = SharedGuildAction::SendModifyGuildMemberOK,
                            .name = request.name,
                            .sender = request.sender,
                            .guildID = request.guildID,
                            .rank = request.requestedRank});

    return EventsResult::Ok(events);
}
