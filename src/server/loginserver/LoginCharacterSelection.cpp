#include "LoginCharacterSelection.h"

#include <iostream>
#include <utility>

#include "CLSelectPC.h"
#include "DatabaseError.h"
#include "LCSelectPCError.h"
#include "LoginIncomingRequest.h"
#include "LoginPlayer.h"

namespace de {
namespace {

BYTE refusalCode(SelectPCRejection rejection) {
    switch (rejection) {
    case SelectPCRejection::DidNotAgree:
        return SELECT_PC_DIDNOT_AGREE;
    case SelectPCRejection::FreePlayLimit:
    case SelectPCRejection::NonPKServerLimit:
        return SELECT_PC_CANNOT_PLAY_BY_ATTR;
    case SelectPCRejection::InvalidStatus:
        throw DisconnectException("invalid player status");
    case SelectPCRejection::NoSuchCharacter:
        throw InvalidProtocolException("no such PC exist.");
    case SelectPCRejection::NoSlot:
        throw InvalidProtocolException("no slot exist.");
    }
    throw Error("unknown character selection rejection");
}

} // namespace

const LoginCharacterSelectionActions& defaultLoginCharacterSelectionActions() {
    static const LoginCharacterSelectionActions actions{
        +[](LoginPlayer& player, LCSelectPCError& packet) { player.sendPacket(&packet); },
        {+[](WorldID_t world, ServerGroupID_t group) {
             std::cout << "WorldID:" << int(world) << " ServerGroupID:" << int(group) << std::endl;
         },
         +[](WorldID_t world, ServerGroupID_t group, ServerID_t server) {
             std::cout << "WorldID " << int(world) << ", ServerGroupID : " << int(group)
                       << ", ServerID : " << int(server) << std::endl;
         }}};
    return actions;
}

bool selectLoginCharacter(LoginPlayer& player, const CLSelectPC& packet, SelectPCTopology& topology,
                          const LoginIncomingRequestServices& incoming, const LoginCharacterSelectionActions& actions,
                          const LoginCharacterSelectionRules& rules) {
    SelectPCRequest request;
    request.worldID = player.getWorldID();
    request.serverGroupID = player.getServerGroupID();
    request.playerID = player.getID();
    request.pcName = packet.getPCName();
    request.pcType = packet.getPCType();
    request.inCharacterManagement = player.getPlayerStatus() == LPS_PC_MANAGEMENT;
    request.agreedToTerms = rules.agreedToTerms;
    request.checkFreePlayLimit = rules.checkFreePlayLimit;
    request.freePlaySlayerDomainSum = rules.freePlaySlayerDomainSum;
    request.freePlayVampireLevel = rules.freePlayVampireLevel;
    try {
        auto result = decideSelectPC(request, incoming.characters, topology, actions.diagnostics);
        if (result.isRejected()) {
            LCSelectPCError refusal;
            refusal.setCode(refusalCode(result.rejection()));
            actions.sendRefusal(player, refusal);
            return false;
        }
        const auto selected = std::move(result).events();
        requestLoginIncomingConnection(player, request, selected, incoming);
        return true;
    } catch (const DatabaseError& error) {
        throw DisconnectException("CLSelectPCHandler : " + error.message());
    } catch (const NoSuchElementException& error) {
        throw Error("Critical Error : data intergrity broken at ZoneInfo - ZoneGroupInfo - GameServerInfo : " +
                    error.toString());
    }
}

} // namespace de
