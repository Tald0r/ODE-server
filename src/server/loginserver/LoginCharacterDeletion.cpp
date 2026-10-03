#include "LoginCharacterDeletion.h"

#include <iostream>

#include "CLDeletePC.h"
#include "DatabaseError.h"
#include "LCDeletePCError.h"
#include "LCDeletePCOK.h"
#include "LoginPlayer.h"
#include "Utility.h"

namespace de {
namespace {

template <typename Callback, typename... Args> void report(const Callback& callback, const Args&... args) noexcept {
    if (!callback)
        return;
    try {
        callback(args...);
    } catch (...) {
    }
}

BYTE refusalCode(DeletePCRejection rejection) {
    switch (rejection) {
    case DeletePCRejection::NoSuchCharacter:
        return NOT_FOUND_PLAYER;
    case DeletePCRejection::NotTheOwner:
        return 0;
    case DeletePCRejection::SlotMismatch:
        return NOT_FOUND_ID;
    }
    throw Error("unknown character deletion rejection");
}

} // namespace

const LoginCharacterDeletionActions& defaultLoginCharacterDeletionActions() {
    static const LoginCharacterDeletionActions actions{
        +[](LoginPlayer& player, LCDeletePCError& packet) { player.sendPacket(&packet); },
        +[](LoginPlayer& player, LCDeletePCOK& packet) { player.sendPacket(&packet); },
        {+[](const CLDeletePC& packet) { std::cout << packet.toString() << std::endl; },
         +[](const DeletePCRequest& request) {
             filelog("DeletePC.log", "Illegal PC Delete : [%s:%s]", request.playerID.c_str(), request.name.c_str());
         },
         +[](DeletePCRejection rejection) {
             if (rejection == DeletePCRejection::NotTheOwner)
                 std::cout << "Fail to deletePC : illegal pc delete" << std::endl;
             else
                 std::cout << "Fail to deletePC : no such slayer exist." << std::endl;
         },
         +[](const std::string& message) { std::cout << "Fail to deletePC : " << message << std::endl; }}};
    return actions;
}

bool deleteLoginCharacter(LoginPlayer& player, const CLDeletePC& packet, LoginCharacterPurgeRepository& repository,
                          const LoginCharacterDeletionActions& actions) {
    DeletePCRequest request;
    request.worldID = player.getWorldID();
    request.playerID = player.getID();
    request.name = packet.getName();
    request.slot = packet.getSlot();
    LCDeletePCError refusal;
    report(actions.diagnostics.request, packet);
    try {
        auto result = decideDeletePC(request, repository);
        if (result.isRejected()) {
            const auto rejection = result.rejection();
            refusal.setErrorID(refusalCode(rejection));
            if (rejection == DeletePCRejection::NotTheOwner)
                report(actions.diagnostics.wrongOwner, request);
            report(actions.diagnostics.refusal, rejection);
            actions.sendRefusal(player, refusal);
            return false;
        }
        repository.recordDeletion(request.playerID, request.worldID, request.name);
        repository.purgeCharacterRows(request.worldID, request.name, request.slot);
        LCDeletePCOK success;
        actions.sendSuccess(player, success);
        player.setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        return true;
    } catch (const DatabaseError& error) {
        report(actions.diagnostics.databaseFailure, error.message());
        actions.sendRefusal(player, refusal);
        return false;
    }
}

} // namespace de
