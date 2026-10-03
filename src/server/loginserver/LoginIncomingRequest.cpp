#include "LoginIncomingRequest.h"

#include <utility>

#include <string_view>

#include "CharacterSelection.h"
#include "GameServerInfoManager.h"
#include "LGIncomingConnection.h"
#include "LoginDatagramSend.h"
#include "LoginPlayer.h"
#include "Properties.h"
#include "ServerPortSettings.h"
#include "Socket.h"
#include "repository/LoginAccountRepository.h"

namespace de {
namespace {

void validateField(std::string_view value, std::size_t maximum, const char* name) {
    if (value.empty() || value.size() > maximum)
        throw InvalidProtocolException(std::string("invalid incoming connection ") + name + " length");
}

} // namespace

void requestLoginIncomingConnection(LoginPlayer& player, const SelectPCRequest& request,
                                    const SelectedCharacter& selected, const LoginIncomingRequestServices& services) {
    if (player.getPlayerStatus() != LPS_PC_MANAGEMENT || !request.inCharacterManagement ||
        player.getID() != request.playerID || player.getWorldID() != request.worldID ||
        player.getServerGroupID() != request.serverGroupID || request.playerID == "NONE")
        throw DisconnectException("stale character selection");
    if (selected.slot < 1 || selected.slot > 3)
        throw InvalidProtocolException("invalid selected character slot");

    const auto snapshot = request;
    const auto slot = selected.slot;
    const auto clientIP = player.getSocket()->getHost();
    validateField(snapshot.playerID, 20, "account");
    validateField(snapshot.pcName, 20, "character");
    validateField(clientIP, 15, "client IP");
    const auto* server =
        services.servers.getGameServerInfo(selected.serverID, snapshot.serverGroupID, snapshot.worldID);
    const auto host = server->getIP();
    const auto user = services.config.getProperty("User");
    uint port;
    try {
        port = user == "excel96" ? server->getUDPPort() : readServerPort(services.config, "GameServerUDPPort");
        validateLoginDatagramEndpoint(host, port);
    } catch (const Error& error) {
        // Match the production sender's connection-failure boundary, so a bad
        // destination retires this session instead of escaping the client loop.
        throw ConnectException("invalid incoming connection destination: " + error.toString());
    }

    LGIncomingConnection packet;
    packet.setClientIP(clientIP);
    packet.setPlayerID(snapshot.playerID);
    packet.setPCName(snapshot.pcName);
    if (user == "elcastle" && services.reportDestination) {
        try {
            services.reportDestination(host, port);
        } catch (...) {
        }
    }

    const auto previousStatus = player.getPlayerStatus();
    auto previousIP = player.exchangeGameServerIP(host);
    player.setPlayerStatus(LPS_AFTER_SENDING_LG_INCOMING_CONNECTION);
    try {
        services.send(host, port, packet);
    } catch (...) {
        player.exchangeGameServerIP(std::move(previousIP));
        player.setPlayerStatus(previousStatus);
        throw;
    }
    services.accounts.setCurrentLocation(snapshot.worldID, snapshot.serverGroupID, slot, snapshot.playerID);
    services.characters.setCharacterServerGroup(snapshot.worldID, snapshot.serverGroupID, snapshot.pcName);
}

} // namespace de
