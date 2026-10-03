//////////////////////////////////////////////////////////////////////////////
// Filename    : CLRegisterPlayerHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLRegisterPlayer.h"

#ifdef __LOGIN_SERVER__
#include <utility>

#include "Assert1.h"
#include "DatabaseError.h"
#include "GameServerGroupInfoManager.h"
#include "KernelContext.h"
#include "LCRegisterPlayerError.h"
#include "LCRegisterPlayerOK.h"
#include "LoginAccountSession.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "Properties.h"
#include "Registration.h"
#include "repository/LoginAccountRepository.h"
#endif

#ifdef __LOGIN_SERVER__
namespace {

// A registration may fail this many times on one connection before it is
// dropped.
const uint kMaxFailure = 3;

// The LCRegisterPlayerError code each refusal answers with.
BYTE errorIDFor(RegisterPlayerRejection reason) {
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
        break;
    }

    return ETC_ERROR;
}

// A registration that failed but left the connection standing: the session
// waits for another CLRegisterPlayer until the failure count runs out.
void countFailureOrDisconnect(LoginPlayer* pLoginPlayer) {
    uint nFailed = pLoginPlayer->getFailureCount() + 1;
    if (nFailed > kMaxFailure)
        throw DisconnectException("too many failure");
    pLoginPlayer->setFailureCount(nFailed);

    pLoginPlayer->setPlayerStatus(LPS_WAITING_FOR_CL_REGISTER_PLAYER);
}

} // namespace
#endif

//////////////////////////////////////////////////////////////////////////////
// A fresh connection (LPS_BEGIN_SESSION) may register an account: validate
// the packet, insert the Player row, and answer with LCRegisterPlayerOK or
// LCRegisterPlayerError. On success the session is logged in as the new
// account and continues like a normal login (world list, PC list, ...).
//////////////////////////////////////////////////////////////////////////////
void CLRegisterPlayerHandler::execute(CLRegisterPlayer* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    __BEGIN_DEBUG

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    LCRegisterPlayerError lcRegisterPlayerError;
    LoginAccountRepository& repo = defaultLoginAccountRepository();

    RegisterPlayerRequest request;
    request.playerID = pPacket->getID();
    request.password = pPacket->getPassword();
    request.name = pPacket->getName();
    request.sex = pPacket->getSex();
    request.ssn = pPacket->getSSN();
    request.telephone = pPacket->getTelephone();
    request.cellular = pPacket->getCellular();
    request.zipCode = pPacket->getZipCode();
    request.address = pPacket->getAddress();
    request.nation = (int)pPacket->getNation();
    request.email = pPacket->getEmail();
    request.homepage = pPacket->getHomepage();
    request.profile = pPacket->getProfile();
    request.publicProfile = pPacket->getPublic();

    try {
        Outcome<LoginNewAccount, RegisterPlayerRefusal> outcome = decideRegisterPlayer(request, repo);

        if (outcome.isRejected()) {
            const RegisterPlayerRefusal rejection = std::move(outcome).rejection();

            lcRegisterPlayerError.setErrorID(errorIDFor(rejection.reason));
            pLoginPlayer->sendPacket(&lcRegisterPlayerError);

            if (rejection.reason == RegisterPlayerRejection::AlreadyRegistered) {
                countFailureOrDisconnect(pLoginPlayer);
                return;
            }

            if (rejection.reason == RegisterPlayerRejection::PasswordHashingFailed) {
                filelog("loginfail.txt", "Password hashing failed, PlayerID : %s : %s", request.playerID.c_str(),
                        rejection.detail.c_str());
                throw DisconnectException("password hashing failed");
            }

            // For now disconnect the client on validation failure.
            // *TODO* Allow guest to retry without full disconnect.
            throw DisconnectException(lcRegisterPlayerError.toString());
        }

        const LoginNewAccount account = std::move(outcome).events();

        de::registerLoginAccount(pLoginPlayer->loginAccountOwnership(), repo, account,
                                 pLoginPlayer->getSocket()->getHost(),
                                 de::kernelContext().config().getPropertyInt("LoginServerID"));

        int currentWorldID = 0;
        int currentServerGroupID = 0;
        if (!repo.loadCurrentLocation(LOGIN_LOCATION_SQL_UPPER, request.playerID, currentWorldID,
                                      currentServerGroupID)) {
            // The row that was just inserted could not be read back.
            lcRegisterPlayerError.setErrorID(ETC_ERROR);
            pLoginPlayer->sendPacket(&lcRegisterPlayerError);

            countFailureOrDisconnect(pLoginPlayer);
            return;
        }

        WorldID_t WorldID = currentWorldID;
        ServerGroupID_t ServerGroupID = currentServerGroupID;

        pLoginPlayer->setServerGroupID(ServerGroupID);

        LCRegisterPlayerOK lcRegisterPlayerOK;
        lcRegisterPlayerOK.setGroupName(
            de::loginContext().gameServerGroups().getGameServerGroupInfo(ServerGroupID, WorldID)->getGroupName());
        lcRegisterPlayerOK.setAdult(true);
        pLoginPlayer->sendPacket(&lcRegisterPlayerOK);

        pLoginPlayer->setID(request.playerID);
        pLoginPlayer->setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
    } catch (const DatabaseError&) {
        // A SQL failure arrives as END_DB's DatabaseError, already logged to
        // DBError.log; answered like the read-back failure above.
        lcRegisterPlayerError.setErrorID(ETC_ERROR);
        pLoginPlayer->sendPacket(&lcRegisterPlayerError);

        countFailureOrDisconnect(pLoginPlayer);
    }
    __END_DEBUG

#endif

    __END_DEBUG_EX __END_CATCH
}
