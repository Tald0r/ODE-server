#ifndef DARKEDEN_LOGIN_CHARACTER_SELECTION_H
#define DARKEDEN_LOGIN_CHARACTER_SELECTION_H

#include <functional>

#include "CharacterSelection.h"

class CLSelectPC;
class LCSelectPCError;
class LoginPlayer;

namespace de {

struct LoginIncomingRequestServices;

// Production leaves the external terms/billing gates disabled. Explicit rules
// let other callers supply the same decision inputs without process settings.
struct LoginCharacterSelectionRules {
    bool agreedToTerms = true;
    bool checkFreePlayLimit = false;
    int freePlaySlayerDomainSum = 0;
    int freePlayVampireLevel = 0;
};

struct LoginCharacterSelectionActions {
    std::function<void(LoginPlayer&, LCSelectPCError&)> sendRefusal;
    SelectPCDiagnostics diagnostics;
};

const LoginCharacterSelectionActions& defaultLoginCharacterSelectionActions();

// Snapshot the packet/session, decide, then send a refusal or dispatch the
// accepted incoming request. Return true only after dispatch and both writes;
// a sent refusal returns false without changing the session. Preserve legacy
// fatal rejection messages, DatabaseError -> DisconnectException and missing
// data -> Error translation. Other failures retain their identity. Partial
// dispatch/write effects and account ownership follow LoginIncomingRequest.
// Callers serialize access with replies and retain the player's ownership.
// Callbacks are synchronous and must not mutate or retire the player.
[[nodiscard]] bool selectLoginCharacter(LoginPlayer& player, const CLSelectPC& packet, SelectPCTopology& topology,
                                        const LoginIncomingRequestServices& incoming,
                                        const LoginCharacterSelectionActions& actions,
                                        const LoginCharacterSelectionRules& rules = {});

} // namespace de

#endif
