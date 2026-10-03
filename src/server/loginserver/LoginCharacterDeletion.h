#ifndef DARKEDEN_LOGIN_CHARACTER_DELETION_H
#define DARKEDEN_LOGIN_CHARACTER_DELETION_H

#include <functional>

#include "CharacterDeletion.h"

class CLDeletePC;
class LCDeletePCError;
class LCDeletePCOK;
class LoginPlayer;

namespace de {

struct LoginCharacterDeletionDiagnostics {
    std::function<void(const CLDeletePC&)> request;
    std::function<void(const DeletePCRequest&)> wrongOwner;
    std::function<void(DeletePCRejection)> refusal;
    std::function<void(const std::string&)> databaseFailure;
};

struct LoginCharacterDeletionActions {
    std::function<void(LoginPlayer&, LCDeletePCError&)> sendRefusal;
    std::function<void(LoginPlayer&, LCDeletePCOK&)> sendSuccess;
    LoginCharacterDeletionDiagnostics diagnostics;
};

const LoginCharacterDeletionActions& defaultLoginCharacterDeletionActions();

// Snapshot decoded packet/session inputs, decide (including Slayer retirement),
// record and purge, then send success and advance the phase. Return true only
// after the complete sequence; a sent refusal returns false. DatabaseError
// triggers one refusal with the current code (initially zero); errors sending
// that refusal propagate. Other failures retain their identity. Earlier writes
// are not rolled back; the caller retains player/account cleanup ownership.
// Diagnostics are optional, independently best effort, and cannot change results.
// Callers serialize player access; synchronous callbacks must not mutate or
// retire the player. Packet slots must satisfy the decoder's range contract.
[[nodiscard]] bool deleteLoginCharacter(LoginPlayer& player, const CLDeletePC& packet,
                                        LoginCharacterPurgeRepository& repository,
                                        const LoginCharacterDeletionActions& actions);

} // namespace de

#endif
