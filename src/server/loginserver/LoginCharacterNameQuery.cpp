#include "LoginCharacterNameQuery.h"

#include "Assert.h"
#include "CLQueryCharacterName.h"
#include "CharacterCreation.h"
#include "GameWorldInfoManager.h"
#include "LCQueryResultCharacterName.h"
#include "LoginPlayer.h"
#include "repository/LoginCharacterRepository.h"

namespace de {

const LoginCharacterNameQueryActions& defaultLoginCharacterNameQueryActions() {
    static const LoginCharacterNameQueryActions actions{
        +[](LoginPlayer& player, LCQueryResultCharacterName& reply) { player.sendPacket(&reply); }};
    return actions;
}

void queryLoginCharacterName(LoginPlayer& player, const CLQueryCharacterName& packet,
                             const GameWorldInfoManager& worlds, LoginCharacterRepository& repository,
                             const LoginCharacterNameQueryActions& actions) {
    const auto worldID = player.getWorldID();
    Assert(worlds.getGameWorldInfos().contains(worldID));
    const auto name = packet.getCharacterName();
    const bool exists = repository.slayerNameExists(worldID, name);
    LCQueryResultCharacterName reply;
    reply.setCharacterName(name);
    reply.setExist(exists);
    if (!isAvailableID(name.c_str()))
        reply.setExist(true);
    actions.send(player, reply);
    player.setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
}

} // namespace de
