#include "LoginRegistration.h"

#include <utility>

#include "CLRegisterPlayer.h"
#include "DatabaseError.h"
#include "GameServerGroupInfoManager.h"
#include "KernelContext.h"
#include "LCRegisterPlayerError.h"
#include "LCRegisterPlayerOK.h"
#include "LoginAccountSession.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "PasswordHash.h"
#include "Properties.h"
#include "Utility.h"

namespace de {
namespace {

BYTE refusalCode(RegisterPlayerRejection reason) {
    switch (reason) {
    case RegisterPlayerRejection::EmptyID:
        return EMPTY_ID;
    case RegisterPlayerRejection::ShortID:
        return SMALL_ID_LENGTH;
    case RegisterPlayerRejection::EmptyPassword:
        return EMPTY_PASSWORD;
    case RegisterPlayerRejection::ShortPassword:
        return SMALL_PASSWORD_LENGTH;
    case RegisterPlayerRejection::EmptyName:
        return EMPTY_NAME;
    case RegisterPlayerRejection::EmptySSN:
        return EMPTY_SSN;
    case RegisterPlayerRejection::AlreadyRegistered:
        return ALREADY_REGISTER_ID;
    case RegisterPlayerRejection::InvalidID:
    case RegisterPlayerRejection::InvalidProfileField:
    case RegisterPlayerRejection::PasswordHashingFailed:
        return ETC_ERROR;
    }
    throw Error("unknown registration rejection");
}

void countFailureOrDisconnect(LoginPlayer& player) {
    const auto failed = player.getFailureCount();
    if (failed >= 3)
        throw DisconnectException("too many failure");
    player.setFailureCount(failed + 1);
    player.setPlayerStatus(LPS_WAITING_FOR_CL_REGISTER_PLAYER);
}

} // namespace

const LoginRegistrationActions& defaultLoginRegistrationActions() {
    static const LoginRegistrationActions actions{
        {password::hash},
        +[](LoginPlayer& player) {
            return LoginRegistrationConnection{player.getSocket()->getHost(),
                                               kernelContext().config().getPropertyInt("LoginServerID")};
        },
        +[](WorldID_t world, ServerGroupID_t group) {
            return loginContext().gameServerGroups().getGameServerGroupInfo(group, world)->getGroupName();
        },
        +[](LoginPlayer& player, LCRegisterPlayerError& reply) { player.sendPacket(&reply); },
        +[](LoginPlayer& player, LCRegisterPlayerOK& reply) { player.sendPacket(&reply); },
        +[](const std::string& account, const std::string& detail) {
            filelog("loginfail.txt", "Password hashing failed, PlayerID : %s : %s", account.c_str(), detail.c_str());
        }};
    return actions;
}

bool registerLoginPlayer(LoginPlayer& player, const CLRegisterPlayer& packet, LoginAccountRepository& repository,
                         const LoginRegistrationActions& actions) {
    RegisterPlayerRequest request;
    request.playerID = packet.getID();
    request.password = packet.getPassword();
    request.name = packet.getName();
    request.sex = packet.getSex();
    request.ssn = packet.getSSN();
    request.telephone = packet.getTelephone();
    request.cellular = packet.getCellular();
    request.zipCode = packet.getZipCode();
    request.address = packet.getAddress();
    request.nation = static_cast<int>(packet.getNation());
    request.email = packet.getEmail();
    request.homepage = packet.getHomepage();
    request.profile = packet.getProfile();
    request.publicProfile = packet.getPublic();

    LCRegisterPlayerError refusal;
    try {
        auto result = decideRegisterPlayer(request, repository, actions.decision);
        if (result.isRejected()) {
            const auto rejection = std::move(result).rejection();
            refusal.setErrorID(refusalCode(rejection.reason));
            actions.sendRefusal(player, refusal);
            if (rejection.reason == RegisterPlayerRejection::AlreadyRegistered) {
                countFailureOrDisconnect(player);
                return false;
            }
            if (rejection.reason == RegisterPlayerRejection::PasswordHashingFailed) {
                try {
                    if (actions.hashingFailure)
                        actions.hashingFailure(request.playerID, rejection.detail);
                } catch (...) {
                }
                throw DisconnectException("password hashing failed");
            }
            throw DisconnectException(refusal.toString());
        }

        const auto account = std::move(result).events();
        const auto connection = actions.connection(player);
        registerLoginAccount(player.loginAccountOwnership(), repository, account, connection.ip,
                             connection.loginServerID);

        int currentWorld = 0;
        int currentGroup = 0;
        if (!repository.loadCurrentLocation(LOGIN_LOCATION_SQL_UPPER, request.playerID, currentWorld, currentGroup)) {
            refusal.setErrorID(ETC_ERROR);
            actions.sendRefusal(player, refusal);
            countFailureOrDisconnect(player);
            return false;
        }
        if (!std::in_range<WorldID_t>(currentWorld))
            throw Error("invalid registration world ID");
        if (!std::in_range<ServerGroupID_t>(currentGroup))
            throw Error("invalid registration group ID");
        const auto world = static_cast<WorldID_t>(currentWorld);
        const auto group = static_cast<ServerGroupID_t>(currentGroup);
        LCRegisterPlayerOK reply;
        reply.setGroupName(actions.groupName(world, group));
        reply.setAdult(true);
        actions.sendSuccess(player, reply);
        player.publishRegistration(std::move(request.playerID), world, group);
        return true;
    } catch (const DatabaseError&) {
        refusal.setErrorID(ETC_ERROR);
        actions.sendRefusal(player, refusal);
        countFailureOrDisconnect(player);
        return false;
    }
}

} // namespace de
