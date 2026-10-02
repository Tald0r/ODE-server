//////////////////////////////////////////////////////////////////////////////
// Filename    : CharacterSelection.h
// Description : the loginserver's character-selection decision, separated
//               from the CLSelectPC handler so it can be exercised without
//               a socket, a database or a game server.
//////////////////////////////////////////////////////////////////////////////

#ifndef __CHARACTER_SELECTION_H__
#define __CHARACTER_SELECTION_H__

#include <string>

#include "Outcome.h"
#include "Types.h"
#include "repository/LoginCharacterRepository.h"

// Why a selection was refused.
//
//   DidNotAgree        LCSelectPCError(SELECT_PC_DIDNOT_AGREE)
//   FreePlayLimit      LCSelectPCError(SELECT_PC_CANNOT_PLAY_BY_ATTR)
//   NonPKServerLimit   LCSelectPCError(SELECT_PC_CANNOT_PLAY_BY_ATTR)
//   InvalidStatus      the connection is dropped
//   NoSuchCharacter    the connection is dropped
//   NoSlot             the connection is dropped
//
// The last three are not producible by a client that speaks the protocol,
// so the handler drops the connection instead of answering with an error
// packet.
enum class SelectPCRejection {
    // The account has not accepted the terms yet. Only the Netmarble
    // build asks, so only it produces this.
    DidNotAgree,
    // The session is not at the character-management step, so a
    // selection is not the packet that belongs here.
    InvalidStatus,
    // No ACTIVE row of that name in that race table on that account.
    NoSuchCharacter,
    // The character is past what an account without a paid subscription
    // may bring into the game. Only a build with the free-play cap sets
    // the limits, so only it produces this.
    FreePlayLimit,
    // A non-PK server refuses a high-level character of the top
    // competence.
    NonPKServerLimit,
    // The character's stored slot text is not SLOT1, SLOT2 or SLOT3.
    NoSlot
};

// A selection request: the CLSelectPC packet's fields plus the session
// state the handler reads off the player.
struct SelectPCRequest {
    WorldID_t worldID = 0;
    ServerGroupID_t serverGroupID = 0;
    std::string playerID;
    std::string pcName;
    // CLSelectPC::read refuses any byte that names no PCType, so this is
    // always one of the three.
    PCType pcType = PC_SLAYER;
    // Is the session at the character-management step (LPS_PC_MANAGEMENT)?
    bool inCharacterManagement = false;
    // Has the account accepted the terms? Builds that ask nothing leave
    // this true.
    bool agreedToTerms = true;
    // Does this account still play under the free-play cap? The two
    // limits are the configured maxima: the sum of a Slayer's skill
    // domains, and a Vampire's or Ousters' level.
    bool checkFreePlayLimit = false;
    int freePlaySlayerDomainSum = 0;
    int freePlayVampireLevel = 0;
};

// The character an accepted selection hands to the game-server handshake.
struct SelectedCharacter {
    // The race table the row was read from.
    LoginRaceTable table = LOGIN_RACE_TABLE_SLAYER;
    // Where the character logged out, and the game server that runs it.
    ZoneID_t zoneID = 0;
    ServerID_t serverID = 0;
    // The one-based slot (1..3), stored as the account's last played slot.
    // Packet and creation Slot enumerators instead start at zero.
    int slot = 0;
};

// The server topology a selection reads. Both lookups throw
// NoSuchElementException when the tables do not describe the world the
// account is on; that is a configuration fault, not a player-facing
// rejection, and is left to the caller.
class SelectPCTopology {
public:
    virtual ~SelectPCTopology() {}

    // Does the account's own server group run without player killing?
    // (The first game server of the group carries the flag.)
    virtual bool isNonPKServer(WorldID_t worldID, ServerGroupID_t serverGroupID) = 0;
    // The id of the game server that runs a zone, through the zone's
    // zone group.
    virtual ServerID_t zoneServerID(ZoneID_t zoneID) = 0;
};

// Decide whether a character may enter the game, and through which game
// server.
//
// The repository and the topology are passed in because the character row
// and the routing are reads the decision makes; the writes (the account's
// last location, the character's server group) and the handshake with the
// game server stay with the caller, so this function is a pure decision
// over whatever the collaborators answer and needs no database in a test.
//
// Repository failures propagate unchanged to the caller; they are server
// faults, not player-facing rejections.
[[nodiscard]] Outcome<SelectedCharacter, SelectPCRejection>
decideSelectPC(const SelectPCRequest& request, LoginCharacterRepository& repository, SelectPCTopology& topology);

#endif
