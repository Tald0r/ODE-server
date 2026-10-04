#include "LoginFlow.h"

#include <fstream>
#include <iostream>
#include <utility>

#include "CLLogin.h"
#include "DatabaseError.h"
#include "Deployment.h"
#include "KernelContext.h"
#include "LCLoginOK.h"
#include "LoginCompletion.h"
#include "LoginPlayer.h"
#include "Properties.h"
#include "Socket.h"
#include "Utility.h"

namespace de {
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

const LoginFlowActions& defaultLoginFlowActions() {
    static const LoginFlowActions actions{
        defaultLoginAuthenticationActions(),
        +[] {
            return LoginFlowConfiguration{kernelContext().config().getPropertyInt("LoginServerID"),
                                          isNetMarbleDeployment()};
        },
        +[] { return VSDateTime::currentDateTime(); },
        +[](const std::string& account, const std::string& credential, LoginAccountRepository& repository) {
            return checkStoredPassword(account, credential, repository);
        },
        +[](LoginPlayer& player) { player.sendLGKickCharacter(); },
        +[](LoginPlayer& player, LCLoginOK& reply) { player.sendPacket(&reply); }};
    return actions;
}

bool checkNetMarbleLoginGate(LoginPlayer& player, const CLLogin& packet, LoginAccountRepository& repository,
                             const LoginAuthenticationActions& actions) {
    __BEGIN_TRY __BEGIN_DEBUG_EX return authorizeNetMarbleLogin(player, packet, repository, actions);
    __END_DEBUG_EX __END_CATCH return true;
}

void loginPlayer(LoginPlayer& player, CLLogin& packet, LoginAccountRepository& repo, const LoginFlowActions& actions) {
    // External authorization belongs only to this attempt. Reset without
    // allocation before preparing inputs; LOGON cleanup ownership is separate.
    player.setFreePass(false);
    player.setWebLogin(packet.isWebLogin());
    CLLogin requestPacket(packet);
    auto* pPacket = &requestPacket;
    auto* pLoginPlayer = &player;
    // Trim the surrounding whitespace before checking the address.
    pPacket->setID(trim(pPacket->getID()));
    packet.setID(pPacket->getID());

    string connectIP = pLoginPlayer->getSocket()->getHost();
    string ID = pPacket->getID();

    // MAC address setting
    pLoginPlayer->setMacAddress(pPacket->getRareMacAddress());

    // web login
    bool bWebLogin = pPacket->isWebLogin();

    if (isBlockedIP(connectIP, repo)) {
        de::sendLoginRefusal(*pLoginPlayer, pPacket->getID(), LoginRejectReason::IPBlocked, actions.authentication);
        return;
    }

    // The in-house test build sends its account as '#sigi'.
    const bool bTestClient = (ID[0] == '#');

    if (bTestClient) {
        ID = ID.c_str() + 1;
        pPacket->setID(ID);
        packet.setID(ID);
    }

    if (bWebLogin) {
        if (!authorizeWebLogin(player, requestPacket, repo, actions.authentication)) {
            return;
        }
    } else {
        // Ordinary clients pass this gate without external authorization.
        if (!checkNetMarbleLoginGate(player, requestPacket, repo, actions.authentication)) {
            return;
        }
    }

    bool bFreePass = pLoginPlayer->isFreePass();

    if (bTestClient) {
        if (!bWebLogin && bFreePass) {
            // A NetMarble free pass that is not a web login carries one more
            // reserved character in front of the account id.
            ID = ID.c_str() + 1;
            pPacket->setID(ID);
            packet.setID(ID);
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
        const auto configuration = actions.configuration();
        request.loginServerID = configuration.loginServerID;
        request.useNetMarbleAdultFlag = configuration.useNetMarbleAdultFlag;
        if (request.useNetMarbleAdultFlag)
            request.netMarbleAdultFlag = pPacket->isAdult();

        // A web login and a NetMarble free pass carry no password of their
        // own. An id that could not be interpolated safely is not queried
        // at all; the decision refuses it below.
        if (!bWebLogin && !bFreePass && ID.find_first_of("'\\", 0) >= ID.size()) {
            const PasswordCheck check = actions.password(ID, pPacket->getPassword(), repo);
            request.passwordAccepted = check.accepted;

            // A legacy plaintext row, or a hash under older parameters, is
            // rewritten as a current hash on success.
            if (check.rehash)
                repo.updatePassword(check.hash, ID);
        }

        LoginPlayerSession session(pLoginPlayer);

        Outcome<LoginAccepted, LoginRejection> outcome = decideLogin(request, repo, session, actions.clock());

        if (outcome.isRejected()) {
            const LoginRejection rejection = std::move(outcome).rejection();

            de::sendLoginRefusal(*pLoginPlayer, pPacket->getID(), rejection.reason, actions.authentication);

            if (rejection.reason == LoginRejectReason::FreePassAccountMissing) {
                // The account has no row, so its Access column reads empty
                // and the account check answers a second time.
                de::sendLoginRefusal(*pLoginPlayer, pPacket->getID(), LoginRejectReason::AccessNotAllowed,
                                     actions.authentication);
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

            actions.kick(player);

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

            actions.sendSuccess(player, lcLoginOK);
            pLoginPlayer->setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);
        }
    } catch (const DatabaseError& error) {
        // A SQL failure arrives as END_DB's DatabaseError carrying the line
        // it wrote to DBError.log; rethrown as an Error with that line in it.
        throw Error("CLLoginHandler : " + error.message());
    }

    de::recordLogin(repo, ID, connectIP, actions.clock());
}

} // namespace de
