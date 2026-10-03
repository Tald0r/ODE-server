#ifndef DARKEDEN_LOGIN_SERVER_SELECTION_H
#define DARKEDEN_LOGIN_SERVER_SELECTION_H

#include "Types.h"

class LoginCharacterRepository;
class LoginPlayer;
class WorldSelectionTopology;

namespace de {

// Select a configured location and send its character list using explicit
// collaborators. Refusal leaves the session unchanged. Accepted location fields
// precede character lookup; character-management status follows successful send.
// The selected location stays published if character lookup or sending fails.
void selectLoginServer(LoginPlayer& player, ServerGroupID_t requestedGroup, WorldSelectionTopology& topology,
                       LoginCharacterRepository& characters);

} // namespace de

#endif
