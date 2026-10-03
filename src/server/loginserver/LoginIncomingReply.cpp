#include "LoginIncomingReply.h"

#include <mutex>

#include "GLIncomingConnectionError.h"
#include "GLIncomingConnectionOK.h"
#include "LCReconnect.h"
#include "LoginPlayer.h"
#include "LoginPlayerManager.h"

namespace de {
namespace {

LoginPlayer* pendingPlayer(LoginPlayerManager& players, const std::string& account) {
    if (account.empty() || account == "NONE")
        return nullptr;
    LoginPlayer* player;
    try {
        player = players.getPlayer_NOLOCKED(account);
    } catch (const NoSuchElementException&) {
        return nullptr;
    }
    return player->getPlayerStatus() == LPS_AFTER_SENDING_LG_INCOMING_CONNECTION ? player : nullptr;
}

} // namespace

const LoginIncomingReplyActions& defaultLoginIncomingReplyActions() {
    static const LoginIncomingReplyActions actions{
        +[](LoginPlayer& player, LCReconnect& reply) { player.sendPacket(&reply); }, defaultLoginRetirementActions()};
    return actions;
}

bool completeLoginIncomingConnection(LoginPlayerManager& players, const GLIncomingConnectionOK& reply,
                                     LoginRetirementTime now, const LoginIncomingReplyActions& actions) {
    std::lock_guard guard(players);
    auto* player = pendingPlayer(players, reply.getPlayerID());
    if (!player)
        return false;
    const auto descriptor = player->getSocket()->getSOCKET();
    std::exception_ptr failure;
    try {
        // The saved catalogue address is public; the datagram sender may have
        // an internal address behind a gateway or container network.
        LCReconnect reconnect;
        reconnect.setGameServerIP(player->getGameServerIP());
        reconnect.setGameServerPort(reply.getTCPPort());
        reconnect.setKey(reply.getKey());
        actions.send(*player, reconnect);
    } catch (...) {
        failure = std::current_exception();
    }
    return players.retirePlayer_NOLOCKED(descriptor, !failure, now, actions.retirement, failure);
}

bool refuseLoginIncomingConnection(LoginPlayerManager& players, const GLIncomingConnectionError& reply,
                                   LoginRetirementTime now, const LoginIncomingReplyActions& actions) {
    std::lock_guard guard(players);
    auto* player = pendingPlayer(players, reply.getPlayerID());
    if (!player)
        return false;
    return players.retirePlayer_NOLOCKED(player->getSocket()->getSOCKET(), true, now, actions.retirement);
}

} // namespace de
