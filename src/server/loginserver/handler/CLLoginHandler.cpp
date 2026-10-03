//////////////////////////////////////////////////////////////////////////////
// Filename    : CLLoginHandler.cpp
// Written By  : Reiot
// Description :
//
// The client encrypts its account id and password and sends them to the
// login server. The login server reads the account from the database,
// compares them and answers whether the login succeeded.
//
// *CAUTION*
//
// The Player table's LogOn column keeps one account from being in the game
// twice: a row that reads LOGON is taken to be connected already and the
// login is refused. A crashed server therefore has to reset every LogOn
// column to LOGOFF as it comes back up.
//////////////////////////////////////////////////////////////////////////////

#include "CLLogin.h"

#ifdef __LOGIN_SERVER__
#include <time.h>

#include <exception>
#include <utility>

#include <sys/time.h>

#include "Assert1.h"
#include "DatabaseError.h"
#include "Deployment.h"
#include "GameServerGroupInfoManager.h"
#include "GameServerInfoManager.h"
#include "KernelContext.h"
#include "LCLoginError.h"
#include "LCLoginOK.h"
#include "LoginAuthentication.h"
#include "LoginCompletion.h"
#include "LoginDecision.h"
#include "LoginPlayer.h"
#include "Properties.h"
#include "UserInfoManager.h"
#include "repository/LoginAccountRepository.h"
#include "types/ServerType.h"

#endif

#define SYMBOL_TEST_CLIENT '#'       // the in-house test build
#define SYMBOL_NET_MARBLE_CLIENT '@' // a connection coming from NetMarble


#ifdef __LOGIN_SERVER__
namespace {

// LoginSession over the LoginPlayer the login is running for.
class LoginPlayerSession : public LoginSession {
public:
    explicit LoginPlayerSession(LoginPlayer* pLoginPlayer) : m_pLoginPlayer(pLoginPlayer) {}

    de::LoginAccountOwnership& accountOwnership() noexcept override {
        return m_pLoginPlayer->loginAccountOwnership();
    }

    void setServerGroupID(int serverGroupID) override {
        m_pLoginPlayer->setServerGroupID((ServerGroupID_t)serverGroupID);
    }

    void setPayPlayValue(int payType, const string& payPlayDate, int payPlayHours, uint payPlayFlag,
                         const string& familyPayPlayDate) override {
        m_pLoginPlayer->setPayPlayValue((PayType)payType, payPlayDate, payPlayHours, payPlayFlag, familyPayPlayDate);
    }

    const VSDateTime& payPlayAvailableDateTime() const override {
        return m_pLoginPlayer->getPayPlayAvailableDateTime();
    }

    const VSDateTime& familyPayPlayAvailableDateTime() const override {
        return m_pLoginPlayer->getFamilyPayPlayAvailableDateTime();
    }

    bool loginPayPlay(int payType, const string& payPlayDate, int payPlayHours, uint payPlayFlag, const string& ip,
                      const string& playerID) override {
        return m_pLoginPlayer->loginPayPlay((PayType)payType, payPlayDate, payPlayHours, payPlayFlag, ip, playerID);
    }

private:
    LoginPlayer* m_pLoginPlayer;
};

} // namespace
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CLLoginHandler::execute(CLLogin* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    // Trim the surrounding whitespace. by sigi. 2002.12.6
    pPacket->setID(trim(pPacket->getID()));

    string connectIP = pLoginPlayer->getSocket()->getHost();
    string ID = pPacket->getID();

    // MAC address setting
    pLoginPlayer->setMacAddress(pPacket->getRareMacAddress());

    // web login
    bool bWebLogin = pPacket->isWebLogin();

    // set web login player
    if (bWebLogin)
        pLoginPlayer->setWebLogin();

    LoginAccountRepository& repo = defaultLoginAccountRepository();

    if (isBlockedIP(connectIP, repo)) {
        de::sendLoginRefusal(*pLoginPlayer, pPacket->getID(), LoginRejectReason::IPBlocked,
                             de::defaultLoginAuthenticationActions());
        return;
    }

    // The in-house test build sends its account as '#sigi'.
    const bool bTestClient = (ID[0] == SYMBOL_TEST_CLIENT);

    if (bTestClient) {
        ID = ID.c_str() + 1;
        pPacket->setID(ID);
    }

    if (bWebLogin) {
        if (!checkWebLogin(pPacket, pPlayer)) {
            return;
        }
    } else {
        // A connection coming from NetMarble. by sigi. 2002.10.23
        if (!checkNetMarbleClient(pPacket, pPlayer)) {
            return;
        }
    }

    bool bFreePass = pLoginPlayer->isFreePass(); // by sigi. 2002.10.23

    if (bTestClient) {
        if (!bWebLogin && bFreePass) {
            // A NetMarble free pass that is not a web login carries one more
            // reserved character in front of the account id.
            ID = ID.c_str() + 1;
            pPacket->setID(ID);
        }

        // The test client's login is recorded.
        repo.insertTestClientUser(ID, connectIP);
    }

    string SSN = "";
    string zipcode = "";

    try {
        LoginRequest request;
        request.playerID = ID;
        request.connectIP = connectIP;
        request.webLogin = bWebLogin;
        request.freePass = bFreePass;
        request.failureCount = pLoginPlayer->getFailureCount();
        request.loginServerID = de::kernelContext().config().getPropertyInt("LoginServerID");
        request.useNetMarbleAdultFlag = de::isNetMarbleDeployment();
        if (request.useNetMarbleAdultFlag)
            request.netMarbleAdultFlag = pPacket->isAdult();

        // A web login and a NetMarble free pass carry no password of their
        // own. An id that could not be interpolated safely is not queried
        // at all; the decision refuses it below.
        if (!bWebLogin && !bFreePass && ID.find_first_of("'\\", 0) >= ID.size()) {
            const PasswordCheck check = checkStoredPassword(ID, pPacket->getPassword(), repo);
            request.passwordAccepted = check.accepted;

            // A legacy plaintext row, or a hash under older parameters, is
            // rewritten as a current hash on success.
            if (check.rehash)
                repo.updatePassword(check.hash, ID);
        }

        LoginPlayerSession session(pLoginPlayer);

        Outcome<LoginAccepted, LoginRejection> outcome =
            decideLogin(request, repo, session, VSDateTime::currentDateTime());

        if (outcome.isRejected()) {
            const LoginRejection rejection = std::move(outcome).rejection();

            de::sendLoginRefusal(*pLoginPlayer, pPacket->getID(), rejection.reason,
                                 de::defaultLoginAuthenticationActions());

            if (rejection.reason == LoginRejectReason::FreePassAccountMissing) {
                // The account has no row, so its Access column reads empty
                // and the account check answers a second time.
                de::sendLoginRefusal(*pLoginPlayer, pPacket->getID(), LoginRejectReason::AccessNotAllowed,
                                     de::defaultLoginAuthenticationActions());
            }

            if (rejection.touchesFailureCount) {
                // Drop the connection once the failure count is past its
                // limit.
                if (rejection.disconnect) {
                    throw DisconnectException("too many failure");
                }

                pLoginPlayer->setFailureCount(rejection.failureCount);
            }

            if (rejection.beginSession)
                pLoginPlayer->setPlayerStatus(LPS_BEGIN_SESSION);

            return;
        }

        const LoginAccepted accepted = std::move(outcome).events();

        ID = accepted.playerID;
        SSN = accepted.ssn;
        zipcode = accepted.zipCode;

        if (accepted.next == LoginNextStep::KickCharacter) {
            // The client is answered only once the game server has dropped
            // the character, so what the OK packet needs is kept on the
            // session until then.
            pLoginPlayer->setID(ID);
            pLoginPlayer->setSSN(SSN);
            pLoginPlayer->setZipcode(zipcode);
            pLoginPlayer->setAdult(accepted.adult);

            pLoginPlayer->sendLGKickCharacter();

            return;
        }

        if (accepted.next == LoginNextStep::LoginOK) {
            // The account is authenticated, so keep its id on the session.
            pLoginPlayer->setID(ID);

            if (accepted.grantPremiumWeek) {
                repo.extendPayPlayByWeek(ID);
                repo.markPremiumEventReceived(ID);
            }

            auto lcLoginOK = de::makeLoginOK(accepted.adult, accepted.family, accepted.lastDays);

            pLoginPlayer->sendPacket(&lcLoginOK);
            pLoginPlayer->setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        }
    } catch (const DatabaseError& error) {
        // A SQL failure arrives as END_DB's DatabaseError carrying the line
        // it wrote to DBError.log; rethrown as an Error with that line in it.
        throw Error("CLLoginHandler : " + error.message());
    }

    de::recordLogin(repo, ID, connectIP, VSDateTime::currentDateTime());

#endif

    __END_DEBUG_EX __END_CATCH
}


bool CLLoginHandler::checkNetMarbleClient(CLLogin* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX
#ifdef __LOGIN_SERVER__

        return de::authorizeNetMarbleLogin(*dynamic_cast<LoginPlayer*>(pPlayer), *pPacket,
                                           defaultLoginAccountRepository(), de::defaultLoginAuthenticationActions());

#endif
    __END_DEBUG_EX __END_CATCH

        return true;
}


bool CLLoginHandler::checkFreePass(CLLogin* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    return de::verifyNetMarblePassword(*pPacket, defaultLoginAccountRepository(),
                                       de::defaultLoginAuthenticationActions());

#endif

    __END_DEBUG_EX __END_CATCH

        return false;
}

bool CLLoginHandler::checkWebLogin(CLLogin* pPacket, Player* pPlayer) {
    __BEGIN_TRY

#ifdef __LOGIN_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    return de::authorizeWebLogin(*dynamic_cast<LoginPlayer*>(pPlayer), *pPacket, defaultLoginAccountRepository(),
                                 de::defaultLoginAuthenticationActions());

#endif

    __END_CATCH

    return true;
}
