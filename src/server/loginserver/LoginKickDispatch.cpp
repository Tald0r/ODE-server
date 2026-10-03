#include "LoginKickDispatch.h"

#include <iostream>
#include <vector>

#include "GameServerInfoManager.h"
#include "LGKickCharacter.h"
#include "LoginPlayer.h"
#include "Socket.h"

namespace de {

void dispatchLoginKick(LoginPlayer& player, const LoginKickTarget& target, const GameServerInfoManager& servers,
                       const LoginKickSend& send) {
    struct Destination {
        std::string host;
        uint port;
    };
    std::vector<Destination> destinations;
    try {
        for (int group = 0; group < servers.getMaxServerGroupID(); ++group) {
            const auto groupID = static_cast<ServerGroupID_t>(group);
            if (servers.getSize(target.worldID, groupID) == 0)
                continue;
            const auto* info = servers.getGameServerInfo(1, groupID, target.worldID);
            destinations.push_back({info->getIP(), info->getUDPPort()});
        }
    } catch (const NoSuchElementException&) {
        std::cout << "No GameServerInfo" << std::endl;
        player.setID("NONE"); // Disconnect must not log off the existing session.
        return;
    }
    if (destinations.empty()) {
        std::cout << "No GameServerInfo" << std::endl;
        player.setID("NONE");
        return;
    }

    LGKickCharacter packet;
    packet.setID(player.getSocket()->getSOCKET());
    packet.setPCName(target.characterName);
    for (const auto& destination : destinations)
        send(destination.host, destination.port, packet);

    player.setExpireTimeForKickCharacter();
    player.setPlayerStatus(LPS_WAITING_FOR_GL_KICK_VERIFY);
}

} // namespace de
