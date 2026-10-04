//////////////////////////////////////////////////////////////////////////////
// Filename    : LoginDecision.h
// Description : the loginserver's login decision, separated from the CLLogin
//               handler so it can be exercised without a socket or a
//               database.
//////////////////////////////////////////////////////////////////////////////

#ifndef __LOGIN_DECISION_H__
#define __LOGIN_DECISION_H__

#include <functional>
#include <string>

#include <string_view>

#include "LoginAccountOwnership.h"
#include "Outcome.h"
#include "PasswordHash.h"
#include "VSDateTime.h"
#include "repository/LoginAccountRepository.h"

// Why a login was refused. One value per refusing site, so a value names
// both the LCLoginError code the handler replies with and the numbered
// loginfail.txt line it writes:
//
//   IPBlocked                 IP_DENYED           1
//   MalformedID               INVALID_ID_PASSWORD 2
//   UnknownAccountOrPassword  INVALID_ID_PASSWORD 3
//   FreePassAccountMissing    ETC_ERROR           4, then 5
//   AccessNotAllowed          ETC_ERROR           5
//   NotPayAccount             NOT_PAY_ACCOUNT     6
//   AlreadyConnected          ALREADY_CONNECTED   7
//   AlreadyLoggedOnElsewhere  ALREADY_CONNECTED   8
//   NetMarbleAuthorization    INVALID_ID_PASSWORD 9
//   WebLoginKeyMismatch       INVALID_ID_PASSWORD 10
//   WebLoginKeyNotFound       NOT_FOUND_KEY       11
//   WebLoginKeyExpired        KEY_EXPIRED         12
//
// NotPayAccount is produced only by a build with a pay system compiled in
// and FreePassAccountMissing only by a NetMarble free-pass login, so
// neither is possible in this build. Number 9 belongs to the NetMarble
// authentication adapter. Nothing sends CHILDGUARD_DENYED.
enum class LoginRejectReason {
    IPBlocked,
    MalformedID,
    UnknownAccountOrPassword,
    FreePassAccountMissing,
    AccessNotAllowed,
    NotPayAccount,
    AlreadyConnected,
    AlreadyLoggedOnElsewhere,
    WebLoginKeyMismatch,
    WebLoginKeyNotFound,
    WebLoginKeyExpired,
    NetMarbleAuthorization
};

// A refusal, with the session bookkeeping that goes with it. The reply
// packet and its log line go out in every case, including a disconnect.
struct LoginRejection {
    LoginRejectReason reason = LoginRejectReason::MalformedID;

    // The session returns to LPS_BEGIN_SESSION after the reply.
    bool beginSession = false;

    // The two sites that consult the session's failure counter. failureCount
    // is the value to store; disconnect asks for
    // DisconnectException("too many failure") in place of storing it.
    bool touchesFailureCount = false;
    unsigned int failureCount = 0;
    bool disconnect = false;
};

// What an accepted login does next.
enum class LoginNextStep {
    // The account is already in a game from this same address. The game
    // server is asked to drop the character; the client is answered only
    // once it has.
    KickCharacter,
    // Send LCLoginOK.
    LoginOK,
    // The LogOn column held none of LOGOFF / LOGON / GAME, so the login
    // neither replies nor refuses. The shipped schema declares the column
    // as an enum of exactly those three.
    NoReply
};

// A login request: the CLLogin packet's fields after the handler has
// trimmed the id and taken off the test-client marker, plus the session and
// configuration state the decision reads.
struct LoginRequest {
    std::string playerID;
    std::string connectIP;

    // The packet asked for the web login mode.
    bool webLogin = false;
    // The session was admitted without a password of its own: a web login,
    // or the NetMarble free pass.
    bool freePass = false;
    // The stored password accepted the one that was sent. Always true for a
    // login that carries no password.
    bool passwordAccepted = true;

    // The session's failure counter as it stands.
    unsigned int failureCount = 0;

    // conf LoginServerID.
    int loginServerID = 0;

    // conf IsNetMarble: the packet's own adult flag replaces the one the
    // registered SSN implies.
    bool useNetMarbleAdultFlag = false;
    bool netMarbleAdultFlag = false;
};

// What an accepted login tells the session and the OK reply.
struct LoginAccepted {
    LoginNextStep next = LoginNextStep::LoginOK;

    // The account as its row spells it; it replaces the requested id.
    std::string playerID;
    std::string ssn;
    std::string zipCode;
    int currentServerGroupID = 0;

    bool adult = false;

    // LCLoginOK's fields. 0xfffe is "no expiry to report", 0xfffd "a
    // comeback-event week was just granted"; 0xffff is the packet's own
    // default, which a build with an external billing system leaves alone.
    unsigned short lastDays = 0xffff;
    bool family = false;

    // The account still held an unclaimed comeback-event week: the handler
    // extends PayPlayDate by a week and marks the event received.
    bool grantPremiumWeek = false;
};

// The session state the decision itself sets, because the login sets it
// before checks that can still refuse. Implemented over LoginPlayer by the
// handler; a test's implementation records the values and answers the
// deadlines it was given.
class LoginSession {
public:
    virtual ~LoginSession() {}
    virtual de::LoginAccountOwnership& accountOwnership() noexcept = 0;

    virtual void setServerGroupID(int serverGroupID) = 0;

    // Store the account's pay-play columns. The two deadlines below are
    // what this parsed out of the two date columns.
    virtual void setPayPlayValue(int payType, const std::string& payPlayDate, int payPlayHours,
                                 unsigned int payPlayFlag, const std::string& familyPayPlayDate) = 0;
    virtual const VSDateTime& payPlayAvailableDateTime() const = 0;
    virtual const VSDateTime& familyPayPlayAvailableDateTime() const = 0;

    // Start the pay-play session. Only a build with the pay system applied
    // at login calls this; it answers whether the account has paid.
    virtual bool loginPayPlay(int payType, const std::string& payPlayDate, int payPlayHours, unsigned int payPlayFlag,
                              const std::string& ip, const std::string& playerID) = 0;
};

// The verdict on an account's stored password.
struct PasswordCheck {
    // The password matched. False for a wrong password and for an unknown
    // account alike, so the two cannot be told apart.
    bool accepted = false;
    // The stored value should be replaced with `hash`: it was a legacy
    // plaintext row, or a hash under other parameters. False when hashing
    // itself failed, which is logged and otherwise ignored, leaving the
    // stored value for the next login to retry.
    bool rehash = false;
    std::string hash;
};

struct StoredPasswordActions {
    // Required synchronous operations. Callbacks must not retain borrowed
    // arguments or mutate the supplied account/password while checking them.
    std::function<de::password::Verify(std::string_view, std::string_view)> verify;
    std::function<std::string(std::string_view)> hash;
    // Optional best-effort diagnostic; failures cannot replace acceptance.
    // It runs synchronously and its arguments are borrowed for the call.
    // Hash implementations must not put credentials in error details; neither
    // the stored hash nor submitted password is supplied.
    std::function<void(const std::string&, const char*)> rehashFailure;
};

const StoredPasswordActions& defaultStoredPasswordActions();

// Check a password against the account's stored value. An unknown account
// still pays for one verification, so a reply's latency does not tell an
// unknown account from a wrong password.
[[nodiscard]] PasswordCheck checkStoredPassword(const std::string& playerID, const std::string& password,
                                                LoginAccountRepository& repository);
[[nodiscard]] PasswordCheck checkStoredPassword(const std::string& playerID, const std::string& password,
                                                LoginAccountRepository& repository,
                                                const StoredPasswordActions& actions);

// Is the address covered by an IPBlockInfo entry? An entry whose class is
// none of 0, 1 or 2 blocks outright.
[[nodiscard]] bool isBlockedIP(const std::string& ip, LoginAccountRepository& repository);

struct WebLoginKeyActions {
    // Optional diagnostic. Receives only the account, never either credential.
    std::function<void(const std::string&)> mismatch;
};

// Check a web login's key against the one the site stored for the account.
// The key must match and be at most five minutes old, measured by the
// database's own clock against the row's CreateTime. On acceptance the
// caller consumes the key before granting a free pass. Mismatch reporting is
// best effort; it cannot replace the refusal.
[[nodiscard]] Outcome<void, LoginRejection> decideWebLoginKey(const std::string& playerID, const std::string& key,
                                                              LoginAccountRepository& repository,
                                                              const WebLoginKeyActions& actions);

// Is a birthday at least eighteen years before `now`? The birthday is the
// Korean YYMMDD taken off a registration number. It is compared as a plain
// integer against a threshold written in the tm_year form (years since
// 1900), so from 2018 on the threshold has seven digits and every six-digit
// birthday counts as adult, while the longer substrings the Chinese forms
// produce never do.
[[nodiscard]] bool isAdultByBirthday(const std::string& birthday, const VSDateTime& now);

// Decide whether a login may proceed, and with what.
//
// The repository is passed in because every rule the decision applies is a
// database read; the writes stay with the caller, so this function is a
// pure decision over whatever the repository answers and needs no database
// in a test. The one exception is markLoggedOn, a compare-and-set whose
// result is itself one of the refusals.
//
// A repository that fails its query throws (the DB layer's own const
// char*); that is a server fault, not a player-facing refusal, and is left
// to the caller.
[[nodiscard]] Outcome<LoginAccepted, LoginRejection> decideLogin(const LoginRequest& request,
                                                                 LoginAccountRepository& repository,
                                                                 LoginSession& session, const VSDateTime& now);

#endif
