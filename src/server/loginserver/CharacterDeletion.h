//////////////////////////////////////////////////////////////////////////////
// Filename    : CharacterDeletion.h
// Description : the loginserver's character-deletion decision, separated
//               from the CLDeletePC handler so it can be exercised without
//               a socket or a database.
//////////////////////////////////////////////////////////////////////////////

#ifndef __CHARACTER_DELETION_H__
#define __CHARACTER_DELETION_H__

#include <string>

#include "Outcome.h"
#include "Types.h"
#include "repository/LoginCharacterPurgeRepository.h"

// Why a deletion was refused. Each value names the LCDeletePCError code the
// handler answers with:
//
//   NoSuchCharacter   NOT_FOUND_PLAYER
//   NotTheOwner       zero (the packet's default error id)
//   SlotMismatch      NOT_FOUND_ID
//
// NotTheOwner is the one no client can produce by playing: the character
// list a session is given holds only its own characters. It is answered
// with the error packet all the same, and logged to DeletePC.log.
enum class DeletePCRejection {
    // No ACTIVE Slayer row of that name in the world.
    NoSuchCharacter,
    // The row of that name belongs to another account.
    NotTheOwner,
    // The retirement changed no row: the character is in another slot than
    // the packet claims, or it is already retired.
    SlotMismatch
};

// A deletion request: the CLDeletePC packet's fields plus the account the
// session is logged in as.
struct DeletePCRequest {
    WorldID_t worldID = 0;
    std::string playerID;
    std::string name;
    // CLDeletePC::read refuses any byte that names no slot, so this is
    // always one of the three.
    Slot slot = SLOT1;
};

// Decide whether a character may be deleted.
//
// The repository is passed in because the ownership check is a database
// read; the rows a granted deletion purges are written by the caller, so
// this function needs no database in a test. The one exception is
// retireSlayer, a compare-and-set whose result is itself the last of the
// refusals: it retires the Slayer row that indexes the character, and the
// caller purges the rest only once it has.
//
// Repository failures propagate unchanged for the caller to handle. A successful
// retirement is a persistence effect and cannot be rolled back by this decision.
[[nodiscard]] Outcome<void, DeletePCRejection> decideDeletePC(const DeletePCRequest& request,
                                                              LoginCharacterPurgeRepository& repository);

#endif
