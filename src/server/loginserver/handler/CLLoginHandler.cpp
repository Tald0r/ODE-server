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
#include "LoginCompletion.h"
#include "LoginDecision.h"
#include "LoginPlayer.h"
#include "PasswordHash.h"
#include "Properties.h"
#include "UserInfoManager.h"
#include "repository/LoginAccountRepository.h"
#include "types/ServerType.h"

#endif

#define SYMBOL_TEST_CLIENT '#'       // the in-house test build
#define SYMBOL_NET_MARBLE_CLIENT '@' // a connection coming from NetMarble


#ifdef __LOGIN_SERVER__
namespace {

// Rewrites the account's stored password as a fresh argon2id hash. A hashing
// failure is logged and otherwise ignored: the password was already accepted
// against the stored value, and the next login retries. A SQL failure leaves
// as END_DB's DatabaseError.
void storePasswordHash(const string& ID, const string& password) {
    string hashed;
    try {
        hashed = de::password::hash(password);
    } catch (const std::exception& e) {
        filelog("loginfail.txt", "Password rehash failed, PlayerID : %s : %s", ID.c_str(), e.what());
        return;
    }
    defaultLoginAccountRepository().updatePassword(hashed, ID);
}

// The LCLoginError code and the numbered loginfail.txt line each refusal
// answers with. Number 9 is the NetMarble authorization refusal below.
void replyLoginError(LoginPlayer* pLoginPlayer, const string& reportedID, LoginRejectReason reason) {
    BYTE errorID = ETC_ERROR;
    const char* name = "ETC_ERROR";
    int site = 0;

    switch (reason) {
    case LoginRejectReason::IPBlocked:
        errorID = IP_DENYED;
        name = "IP_DENYED";
        site = 1;
        break;
    case LoginRejectReason::MalformedID:
        errorID = INVALID_ID_PASSWORD;
        name = "INVALID_ID_PASSWORD";
        site = 2;
        break;
    case LoginRejectReason::UnknownAccountOrPassword:
        errorID = INVALID_ID_PASSWORD;
        name = "INVALID_ID_PASSWORD";
        site = 3;
        break;
    case LoginRejectReason::FreePassAccountMissing:
        site = 4;
        break;
    case LoginRejectReason::AccessNotAllowed:
        site = 5;
        break;
    case LoginRejectReason::NotPayAccount:
        errorID = NOT_PAY_ACCOUNT;
        name = "NOT_PAY_ACCOUNT";
        site = 6;
        break;
    case LoginRejectReason::AlreadyConnected:
        errorID = ALREADY_CONNECTED;
        name = "ALREADY_CONNECTED";
        site = 7;
        break;
    case LoginRejectReason::AlreadyLoggedOnElsewhere:
        errorID = ALREADY_CONNECTED;
        name = "ALREADY_CONNECTED";
        site = 8;
        break;
    case LoginRejectReason::WebLoginKeyMismatch:
        errorID = INVALID_ID_PASSWORD;
        name = "INVALID_ID_PASSWORD";
        site = 10;
        break;
    case LoginRejectReason::WebLoginKeyNotFound:
        errorID = NOT_FOUND_KEY;
        name = "NOT_FOUND_KEY";
        site = 11;
        break;
    case LoginRejectReason::WebLoginKeyExpired:
        errorID = KEY_EXPIRED;
        name = "KEY_EXPIRED";
        site = 12;
        break;
    }

    LCLoginError lcLoginError;
    lcLoginError.setErrorID(errorID);
    pLoginPlayer->sendPacket(&lcLoginError);

    filelog("loginfail.txt", "Error Code: %s, %d, PlayerID : %s", name, site, reportedID.c_str());
}

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
        replyLoginError(pLoginPlayer, pPacket->getID(), LoginRejectReason::IPBlocked);
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

            replyLoginError(pLoginPlayer, pPacket->getID(), rejection.reason);

            if (rejection.reason == LoginRejectReason::FreePassAccountMissing) {
                // The account has no row, so its Access column reads empty
                // and the account check answers a second time.
                replyLoginError(pLoginPlayer, pPacket->getID(), LoginRejectReason::AccessNotAllowed);
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

        bool isNetmarble = pPacket->isNetmarble();

    if (isNetmarble) // by sigi. 2002.10.23
    {
        LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

        bool bFreePass = checkFreePass(pPacket, pPlayer);

        if (!bFreePass) {
            LCLoginError lcLoginError;
            lcLoginError.setErrorID(INVALID_ID_PASSWORD);
            pLoginPlayer->sendPacket(&lcLoginError);
            filelog("loginfail.txt", "Error Code: INVALID_ID_PASSWORD, 9, PlayerID : %s", pPacket->getID().c_str());

            return false;
        }

        // Some of the checks hand the session a free pass.
        pLoginPlayer->setFreePass(true);
    }

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

    // The NetMarble password is checked against the stored hash; an
    // account with no row is created on the spot with the hashed
    // password. A SQL failure leaves as END_DB's DatabaseError.
    try {
        LoginAccountRepository& repo = defaultLoginAccountRepository();

        string stored;
        if (repo.loadPasswordHash(pPacket->getID(), stored)) {
            const de::password::Verify verdict = de::password::verify(stored, pPacket->getPassword());
            if (verdict != de::password::Verify::Rejected) {
                if (verdict == de::password::Verify::AcceptedRehash)
                    storePasswordHash(pPacket->getID(), pPacket->getPassword());
                return true;
            }
        } else {
            // A new NetMarble user is always admitted. SpecialEventCount
            // starts at 2, as if the event item had already been given.
            cout << "NetMarble New Player: " << pPacket->getID().c_str() << endl;

            string hashed;
            try {
                hashed = de::password::hash(pPacket->getPassword());
            } catch (const std::exception& e) {
                filelog("loginfail.txt", "Password hashing failed, PlayerID : %s : %s", pPacket->getID().c_str(),
                        e.what());
                return false;
            }

            repo.insertNetMarbleAccount(pPacket->getID(), hashed);

            return true;
        }
    } catch (Throwable& t) {
        return false;
    }

#endif

    __END_DEBUG_EX __END_CATCH

        return false;
}

bool CLLoginHandler::checkWebLogin(CLLogin* pPacket, Player* pPlayer) {
    __BEGIN_TRY

#ifdef __LOGIN_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    // The web login key must match the one the site stored for the
    // account, and be at most five minutes old. A SQL failure leaves as
    // END_DB's DatabaseError.
    try {
        LoginAccountRepository& repo = defaultLoginAccountRepository();

        Outcome<void, LoginRejection> outcome = decideWebLoginKey(pPacket->getID(), pPacket->getPassword(), repo);

        if (outcome.isRejected()) {
            replyLoginError(pLoginPlayer, pPacket->getID(), outcome.rejection().reason);
            return false;
        }

        pLoginPlayer->setFreePass(true);

        repo.deleteWebLoginKey(pPacket->getID());
    } catch (Throwable& t) {
        return false;
    }

#endif

    __END_CATCH

    return true;
}
