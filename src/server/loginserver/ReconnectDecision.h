//////////////////////////////////////////////////////////////////////////////
// Filename    : ReconnectDecision.h
// Description : the loginserver's reconnect decision, separated from the
//               CLReconnectLogin handler so it can be exercised without a
//               socket or a database.
//////////////////////////////////////////////////////////////////////////////

#ifndef __RECONNECT_DECISION_H__
#define __RECONNECT_DECISION_H__

#include <string>

#include "LoginAccountOwnership.h"
#include "Outcome.h"
#include "repository/LoginAccountRepository.h"

// Why a reconnect was refused. A client that reaches this packet has
// already been authenticated by the login it is coming from, so none of
// these is answered with an error packet: each one drops the connection,
// with the message the handler builds from the value.
//
//   NoSuchAccount             "ReconnectLogin verify failed: no such player"
//   AlreadyInGame             "ReconnectLogin verify failed: LogOn = <column>"
//   AlreadyLoggedOnElsewhere  "Deny MultiLogin"
//   AccessNotAllowed          "ReconnectLogin verify failed "
//   NotPayAccount             "Pay First!"
//
// NotPayAccount is produced only by a build with the pay system applied at
// login, so it is not possible in this build; it is also the one value the
// handler answers with an InvalidProtocolException rather than a
// DisconnectException.
enum class ReconnectRejectReason {
    // The account has no Player row.
    NoSuchAccount,
    // The LogOn column reads GAME or LOGON: the account is in a game
    // already.
    AlreadyInGame,
    // The LogOn column read LOGOFF but the compare-and-set changed no row,
    // so another session took it first.
    AlreadyLoggedOnElsewhere,
    // The Access column does not read ALLOW.
    AccessNotAllowed,
    // The account has not paid.
    NotPayAccount
};

// A refusal, with what the message it produces needs.
struct ReconnectRejection {
    ReconnectRejectReason reason = ReconnectRejectReason::NoSuchAccount;

    // The LogOn column that refused; only AlreadyInGame reports it.
    std::string logOn;
};

// What an accepted reconnect tells the handler. Both values are also
// written to the session by the decision, because it writes them before
// checks that can still refuse.
struct ReconnectAccepted {
    int worldID = 0;
    int serverGroupID = 0;
};

// The session state the decision itself sets. Implemented over LoginPlayer
// by the handler; a test's implementation records the values.
class ReconnectSession {
public:
    virtual ~ReconnectSession() {}
    virtual de::LoginAccountOwnership& accountOwnership() noexcept = 0;

    // The account's current world and group, set as soon as the row is
    // read and kept even by a refusal after that point.
    virtual void setWorldID(int worldID) = 0;
    virtual void setServerGroupID(int serverGroupID) = 0;

    // The account id the session is logged in as. An account already in a
    // game has it replaced with NONE before the connection is dropped.
    virtual void setID(const std::string& playerID) = 0;

    // Store the account's pay-play columns. The reconnect projection does
    // not select the family column, so this is the four-column form.
    virtual void setPayPlayValue(int payType, const std::string& payPlayDate, int payPlayHours,
                                 unsigned int payPlayFlag) = 0;

    // Start the pay-play session. Only a build with the pay system applied
    // at login calls this; it answers whether the account has paid.
    virtual bool loginPayPlay(int payType, const std::string& payPlayDate, int payPlayHours, unsigned int payPlayFlag,
                              const std::string& ip, const std::string& playerID) = 0;
};

// A reconnect request: the account the handler took off the
// ReconnectLoginInfo it has already verified, plus the connection and
// configuration state the decision reads.
struct ReconnectRequest {
    std::string playerID;
    // Only a build with a pay system reads this.
    std::string connectIP;
    // conf LoginServerID.
    int loginServerID = 0;
};

// Decide whether a reconnecting session may take the account back into
// character management.
//
// The key and expiry check against the ReconnectLoginInfo the client's
// previous server left behind happens before this and stays with the
// caller: it reads no account state. Everything from the Player row on is
// here. The one write is markLoggedOnForReconnect, a compare-and-set whose
// result is itself one of the refusals; the PC list an accepted reconnect
// replies with is the caller's.
//
// A repository that fails its query throws (the DB layer's own const
// char*); that is a server fault, not a player-facing refusal, and is left
// to the caller.
[[nodiscard]] Outcome<ReconnectAccepted, ReconnectRejection>
decideReconnectLogin(const ReconnectRequest& request, LoginAccountRepository& repository, ReconnectSession& session);

#endif
