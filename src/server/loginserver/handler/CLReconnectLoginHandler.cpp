//////////////////////////////////////////////////////////////////////////////
// Filename    : CLReconnectLoginHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLReconnectLogin.h"

#ifdef __LOGIN_SERVER__
#include <utility>

#include "Assert1.h"
#include "DatabaseError.h"
#include "GCDisconnect.h"
#include "GameServerInfoManager.h"
#include "KernelContext.h"
#include "LCPCList.h"
#include "LoginCharacterList.h"
#include "LoginContext.h"
#include "LoginPlayer.h"
#include "OptionInfo.h"
#include "Properties.h"
#include "ReconnectDecision.h"
#include "ReconnectLoginInfoManager.h"
#include "repository/LoginAccountRepository.h"
#include "repository/LoginCharacterRepository.h"

#endif

#ifdef __LOGIN_SERVER__
namespace {
// ReconnectSession over the LoginPlayer the reconnect is running for.
class ReconnectPlayerSession : public ReconnectSession {
public:
    explicit ReconnectPlayerSession(LoginPlayer* pLoginPlayer) : m_pLoginPlayer(pLoginPlayer) {}

    de::LoginAccountOwnership& accountOwnership() noexcept override {
        return m_pLoginPlayer->loginAccountOwnership();
    }

    void setWorldID(int worldID) override {
        m_pLoginPlayer->setWorldID((WorldID_t)worldID);
    }

    void setServerGroupID(int serverGroupID) override {
        m_pLoginPlayer->setServerGroupID((ServerGroupID_t)serverGroupID);
    }

    void setID(const string& playerID) override {
        m_pLoginPlayer->setID(playerID);
    }

    void setPayPlayValue(int payType, const string& payPlayDate, int payPlayHours, uint payPlayFlag) override {
        m_pLoginPlayer->setPayPlayValue((PayType)payType, payPlayDate, payPlayHours, payPlayFlag);
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
// CLReconnectLoginHandler::execute()
//
// This is the first packet a client sends when it moves from the login
// server to a game server, or from one game server to another. The player
// object has just been created and is held by the IncomingPlayerManager.
//
// Any other packet arriving first is taken to be an intrusion attempt, so
// this has to be the first one: the player object keeps the previous packet,
// and it is enough to check that it is NULL.
//
// A packet that does not verify ends the connection.
//////////////////////////////////////////////////////////////////////////////
void CLReconnectLoginHandler::execute(CLReconnectLogin* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);


    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);
    pLoginPlayer->setWorldID(pPacket->isWebLogin());

    string PlayerID;

    //----------------------------------------------------------------------
    // Find the ReconnectLoginInfo this packet belongs to. An intruder has to
    // guess both the key and the character name inside a time window.
    //----------------------------------------------------------------------
    try {
        ReconnectLoginInfoManager& reconnectLogins = de::loginContext().reconnectLogins();

        ReconnectLoginInfo* pReconnectLoginInfo =
            reconnectLogins.getReconnectLoginInfo(pLoginPlayer->getSocket()->getHost());

        PlayerID = pReconnectLoginInfo->getPlayerID();

        // Keep the account id on the session.
        pLoginPlayer->setID(PlayerID);

        // Verify the key.
        if (pPacket->getKey() != pReconnectLoginInfo->getKey())
            throw InvalidProtocolException("invalid key");

        // Compare the current time against the expiry time.
        Timeval currentTime;
        getCurrentTime(currentTime);
        if (pReconnectLoginInfo->getExpireTime() < currentTime) {
            reconnectLogins.deleteReconnectLoginInfo(pReconnectLoginInfo->getClientIP());
            throw InvalidProtocolException("session already expired");
        }

        // Verified, so the ReconnectLoginInfo is spent.
        reconnectLogins.deleteReconnectLoginInfo(pReconnectLoginInfo->getClientIP());
    } catch (NoSuchElementException& nsee) // no ReconnectLoginInfo for that address
    {
        // A client that takes too long between connecting and sending
        // CLReconnectLogin finds its session expired, and is dropped as
        // well. (Stopping in a debugger between the two is enough.)
        GCDisconnect gcDisconnect;
        gcDisconnect.setMessage(nsee.toString());

        pLoginPlayer->sendPacket(&gcDisconnect);

        // Thrown so that IPM::processCommands() above disconnects.
        throw InvalidProtocolException("reconnect login info not found");
    } catch (InvalidProtocolException& ipe) {
        cout << endl
             << "+-----------------------+" << endl
             << "| Level 2 Access Denied |" << endl
             << "+-----------------------+" << endl
             << endl;

        GCDisconnect gcDisconnect;
        gcDisconnect.setMessage(ipe.toString());

        pLoginPlayer->sendPacket(&gcDisconnect);

        // Thrown so that IPM::processCommands() above disconnects.
        throw;
    }

    ServerGroupID_t CurrentServerGroupID = 0;

    try {
        LoginAccountRepository& repo = defaultLoginAccountRepository();

        ReconnectRequest request;
        request.playerID = PlayerID;
        request.connectIP = pLoginPlayer->getSocket()->getHost();
        request.loginServerID = de::kernelContext().config().getPropertyInt("LoginServerID");

        ReconnectPlayerSession session(pLoginPlayer);

        Outcome<ReconnectAccepted, ReconnectRejection> outcome = decideReconnectLogin(request, repo, session);

        if (outcome.isRejected()) {
            const ReconnectRejection rejection = std::move(outcome).rejection();

            switch (rejection.reason) {
            case ReconnectRejectReason::NoSuchAccount:
                throw DisconnectException("ReconnectLogin verify failed: no such player");

            case ReconnectRejectReason::AlreadyInGame: {
                string msg = "ReconnectLogin verify failed: LogOn = ";
                msg += rejection.logOn;
                throw DisconnectException(msg);
            }

            case ReconnectRejectReason::AlreadyLoggedOnElsewhere:
                throw DisconnectException("Deny MultiLogin");

            case ReconnectRejectReason::AccessNotAllowed:
                throw DisconnectException("ReconnectLogin verify failed ");

            case ReconnectRejectReason::NotPayAccount:
                // Only a build with the pay system applied at login
                // produces this, and it leaves as a protocol error rather
                // than a disconnect.
                throw InvalidProtocolException("Pay First!");
            }
        }

        CurrentServerGroupID = (ServerGroupID_t)std::move(outcome).events().serverGroupID;
    } catch (const DatabaseError& error) {
        // A SQL failure arrives as END_DB's DatabaseError carrying the line
        // it wrote to DBError.log; the reason travels with the disconnect.
        throw DisconnectException("CLReconnectLoginHandler : " + error.message());
    }


    pLoginPlayer->setServerGroupID(CurrentServerGroupID);

    pLoginPlayer->setPlayerStatus(LPS_PC_MANAGEMENT);


    //----------------------------------------------------------------------
    // Answer with the PC list.
    //----------------------------------------------------------------------
    auto lcPCList = de::makeLoginCharacterList(pLoginPlayer->getWorldID(), pLoginPlayer->getID(),
                                               defaultLoginCharacterRepository());
    pLoginPlayer->sendPacket(lcPCList.get());
    pLoginPlayer->setPlayerStatus(LPS_PC_MANAGEMENT);


#endif

    __END_DEBUG_EX __END_CATCH
}
