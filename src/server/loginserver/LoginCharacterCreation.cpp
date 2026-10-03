#include "LoginCharacterCreation.h"

#include <cstdlib>
#include <utility>

#include "CLCreatePC.h"
#include "CharacterCreation.h"
#include "DatabaseError.h"
#include "LCCreatePCError.h"
#include "LCCreatePCOK.h"
#include "LoginPlayer.h"
#include "Utility.h"

namespace de {
namespace {

BYTE refusalCode(CreatePCRejection rejection) {
    switch (rejection) {
    case CreatePCRejection::ReservedName:
    case CreatePCRejection::NameTaken:
    case CreatePCRejection::SlotOccupied:
        return ALREADY_REGISTER_ID;
    case CreatePCRejection::DisallowedCharacters:
    case CreatePCRejection::UnknownRace:
        return ETC_ERROR;
    case CreatePCRejection::InvalidAttributes:
        throw InvalidProtocolException("CLCreatePCHandler::too large character attribute");
    case CreatePCRejection::InvalidSlot:
        throw InvalidProtocolException("CLCreatePCHandler::slot out of range");
    case CreatePCRejection::InvalidHairStyle:
        throw InvalidProtocolException("CLCreatePCHandler::hair style out of range");
    }
    throw Error("unknown character creation rejection");
}

} // namespace

const LoginCharacterCreationActions& defaultLoginCharacterCreationActions() {
    static const LoginCharacterCreationActions actions{
        +[](LoginPlayer& player, LCCreatePCError& packet) { player.sendPacket(&packet); },
        +[](LoginPlayer& player, LCCreatePCOK& packet) { player.sendPacket(&packet); },
        {+[] { return static_cast<unsigned>(std::rand()); },
         +[](const CreatePCRequest& request) {
             filelog("CreatePC.log", "Illegal PC Create [%s:%s] : %u/%u/%u", request.playerID.c_str(),
                     request.name.c_str(), static_cast<unsigned>(request.str), static_cast<unsigned>(request.dex),
                     static_cast<unsigned>(request.inte));
         }}};
    return actions;
}

bool createLoginCharacter(LoginPlayer& player, CLCreatePC& packet, LoginCharacterRepository& repository,
                          CreatePCBalanceCache& balance, const LoginCharacterCreationActions& actions) {
    CreatePCRequest request;
    request.worldID = player.getWorldID();
    request.serverGroupID = player.getServerGroupID();
    request.playerID = player.getID();
    request.name = packet.getName();
    request.slot = packet.getSlot();
    request.sex = packet.getSex();
    request.hairStyle = packet.getHairStyle();
    request.hairColor = packet.getHairColor();
    request.skinColor = packet.getSkinColor();
    request.str = packet.getSTR();
    request.dex = packet.getDEX();
    request.inte = packet.getINT();
    request.race = packet.getRace();

    LCCreatePCError refusal;
    try {
        auto result = decideCreatePC(request, repository, balance, actions.decision);
        if (result.isRejected()) {
            refusal.setErrorID(refusalCode(result.rejection()));
            actions.sendRefusal(player, refusal);
            return false;
        }
        const auto created = std::move(result).events();
        packet.setSTR(created.str);
        packet.setDEX(created.dex);
        packet.setINT(created.inte);
        repository.insertSlayer(request.worldID, created.slayer);
        if (created.hasOustersRow)
            repository.insertOusters(request.worldID, created.ousters);
        else
            repository.insertVampire(request.worldID, created.vampire);
        repository.insertFlagSet(request.worldID, created.slayer.name, created.flagSet);
        LCCreatePCOK success;
        actions.sendSuccess(player, success);
        player.setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        return true;
    } catch (const DatabaseError&) {
        refusal.setErrorID(ETC_ERROR);
        actions.sendRefusal(player, refusal);
        return false;
    }
}

} // namespace de
