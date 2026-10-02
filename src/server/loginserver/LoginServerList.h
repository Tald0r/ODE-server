#ifndef DARKEDEN_LOGIN_SERVER_LIST_H
#define DARKEDEN_LOGIN_SERVER_LIST_H

#include "WorldSelection.h"

class LoginAccountRepository;
class Player;

namespace de {

// World selection reads only the saved group. A direct list request reads the
// current location using the existing lower-case SQL spelling.
enum class LoginServerListQuery { CurrentGroup, CurrentLocation };

// Build and synchronously send a server list from explicit inputs. Topology
// membership must remain stable for the operation; populations are read live.
// Send receives a complete reply that remains owned until the call returns or throws.
void sendLoginServerList(Player& player, WorldID_t worldID, ServerListTopology& topology,
                         LoginAccountRepository& accounts, LoginServerListQuery query,
                         const ServerLoadThresholds& thresholds = {});

} // namespace de

#endif
