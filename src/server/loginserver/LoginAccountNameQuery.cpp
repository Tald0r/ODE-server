#include "LoginAccountNameQuery.h"

#include "CLQueryPlayerID.h"
#include "LCQueryResultPlayerID.h"
#include "LoginPlayer.h"
#include "repository/LoginAccountRepository.h"

namespace de {

const LoginAccountNameQueryActions& defaultLoginAccountNameQueryActions() {
    static const LoginAccountNameQueryActions actions{
        +[](LoginPlayer& player, LCQueryResultPlayerID& reply) { player.sendPacket(&reply); }};
    return actions;
}

void queryLoginAccountName(LoginPlayer& player, const CLQueryPlayerID& packet, LoginAccountRepository& repository,
                           const LoginAccountNameQueryActions& actions) {
    const auto name = packet.getPlayerID();
    const bool exists = repository.accountNameExists(name);
    LCQueryResultPlayerID reply;
    reply.setPlayerID(name);
    reply.setExist(exists);
    actions.send(player, reply);
    player.setPlayerStatus(LPS_WAITING_FOR_CL_REGISTER_PLAYER);
}

} // namespace de
