#include "LoginServerSelection.h"

#include <utility>

#include "LCPCList.h"
#include "LoginCharacterList.h"
#include "LoginPlayer.h"
#include "Utility.h"
#include "WorldSelection.h"

namespace de {

void selectLoginServer(LoginPlayer& player, ServerGroupID_t requestedGroup, WorldSelectionTopology& topology,
                       LoginCharacterRepository& characters) {
    const SelectServerRequest request{player.getWorldID(), requestedGroup};
    auto outcome = decideSelectServer(request, topology);

    if (outcome.isRejected()) {
        const auto& rejection = outcome.rejection();
        switch (rejection.reason) {
        case SelectServerReason::ServerClosed:
            filelog("errorLogin.txt", "Server Closed: %d", rejection.serverGroupID);
            throw DisconnectException("ServerClosed");
        case SelectServerReason::NoWorlds:
            filelog("errorLogin.txt", "No worlds configured");
            throw DisconnectException("NoWorlds");
        case SelectServerReason::NoServerGroups:
            filelog("errorLogin.txt", "No server groups configured for selected world");
            throw DisconnectException("NoServerGroups");
        }
    }

    const auto selected = std::move(outcome).events();
    player.setWorldID(selected.worldID);
    player.setServerGroupID(selected.serverGroupID);

    auto reply = makeLoginCharacterList(player.getWorldID(), player.getID(), characters);
    player.sendPacket(reply.get());
    player.setPlayerStatus(LPS_PC_MANAGEMENT);

    // CLChangeServer owns the account's saved-group update.
}

} // namespace de
