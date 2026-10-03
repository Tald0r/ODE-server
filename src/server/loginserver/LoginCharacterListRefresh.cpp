#include "LoginCharacterListRefresh.h"

#include "LCPCList.h"
#include "LoginCharacterList.h"
#include "LoginPlayer.h"

namespace de {

const LoginCharacterListRefreshActions& defaultLoginCharacterListRefreshActions() {
    static const LoginCharacterListRefreshActions actions{
        +[](LoginPlayer& player, LCPCList& reply) { player.sendPacket(&reply); }};
    return actions;
}

void refreshLoginCharacterList(LoginPlayer& player, LoginCharacterRepository& repository,
                               const LoginCharacterListRefreshActions& actions) {
    const auto world = player.getWorldID();
    const auto account = player.getID();
    auto reply = makeLoginCharacterList(world, account, repository);
    actions.send(player, *reply);
    player.setPlayerStatus(LPS_PC_MANAGEMENT);
}

} // namespace de
