//////////////////////////////////////////////////////////////////////////////
// Filename    : CharacterSelection.cpp
// Description : the character-selection decision behind CLSelectPCHandler.
//////////////////////////////////////////////////////////////////////////////

#include "CharacterSelection.h"

#include <iostream>

namespace {

// Quest zones are instanced by the game servers and are not in the login
// server's ZoneInfo table, so a character standing in one is routed to the
// first game server of its group instead of being looked up.
const ZoneID_t kQuestZoneLow = 10000;
const ZoneID_t kQuestZoneHigh = 30000;
const ServerID_t kFirstServerID = 1;

// A non-PK server refuses a character above this level once it has also
// reached the top competence.
const int kNonPKMaxLevel = 80;
const int kNonPKBannedCompetence = 3;

} // namespace

Outcome<SelectedCharacter, SelectPCRejection>
decideSelectPC(const SelectPCRequest& request, LoginCharacterRepository& repository, SelectPCTopology& topology) {
    typedef Outcome<SelectedCharacter, SelectPCRejection> Result;

    // The terms come before anything else the account may do.
    if (!request.agreedToTerms)
        return Result::Rejected(SelectPCRejection::DidNotAgree);

    if (!request.inCharacterManagement)
        return Result::Rejected(SelectPCRejection::InvalidStatus);

    // The packet names the race table the character lives in; the packet
    // reader has already refused any other type.
    LoginRaceTable table = LOGIN_RACE_TABLE_OUSTERS;
    const bool isSlayer = (request.pcType == PC_SLAYER);
    if (isSlayer) {
        table = LOGIN_RACE_TABLE_SLAYER;
    } else if (request.pcType == PC_VAMPIRE) {
        table = LOGIN_RACE_TABLE_VAMPIRE;
    }

    // The ACTIVE character of that name on this account.
    LoginSelectRow pc;
    if (!repository.loadCharacterForSelect(request.worldID, table, request.pcName, request.playerID, pc))
        return Result::Rejected(SelectPCRejection::NoSuchCharacter);

    // An account playing for free may only bring in a character below the
    // configured cap: the sum of the five skill domains for a Slayer, the
    // level for the other two races.
    if (request.checkFreePlayLimit) {
        const int limit = isSlayer ? request.freePlaySlayerDomainSum : request.freePlayVampireLevel;

        if (pc.level > limit)
            return Result::Rejected(SelectPCRejection::FreePlayLimit);
    }

    if (topology.isNonPKServer(request.worldID, request.serverGroupID)) {
        std::cout << "WorldID:" << (int)(request.worldID) << " ServerGroupID:" << (int)(request.serverGroupID)
                  << std::endl;

        if (pc.level > kNonPKMaxLevel && pc.competence == kNonPKBannedCompetence)
            return Result::Rejected(SelectPCRejection::NonPKServerLimit);
    }

    // Creation stores these exact names. The account's last-slot value is
    // one-based, unlike the Slot enumerators used by packets and creation.
    int accountSlot = 0;
    for (int slot = SLOT1; slot < SLOT_MAX; ++slot) {
        if (pc.slot == Slot2String[slot]) {
            accountSlot = slot + 1;
            break;
        }
    }
    if (accountSlot == 0)
        return Result::Rejected(SelectPCRejection::NoSlot);

    SelectedCharacter selected;
    selected.table = table;
    selected.zoneID = pc.zoneID;
    selected.slot = accountSlot;

    // Find the game server that runs the zone the character logged out in.
    if (pc.zoneID > kQuestZoneLow && pc.zoneID < kQuestZoneHigh) {
        selected.serverID = kFirstServerID;
    } else {
        selected.serverID = topology.zoneServerID(pc.zoneID);

        std::cout << "WorldID " << (int)request.worldID << ", ServerGroupID : " << (int)request.serverGroupID
                  << ", ServerID : " << (int)selected.serverID << std::endl;
    }

    return Result::Ok(selected);
}
