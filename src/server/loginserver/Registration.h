//////////////////////////////////////////////////////////////////////////////
// Filename    : Registration.h
// Description : the loginserver's account-registration decision, separated
//               from the CLRegisterPlayer handler so it can be exercised
//               without a socket or a database.
//////////////////////////////////////////////////////////////////////////////

#ifndef __REGISTRATION_H__
#define __REGISTRATION_H__

#include <functional>
#include <string>

#include "Outcome.h"
#include "Types.h"
#include "repository/LoginAccountRepository.h"

// Why a registration was refused. Each value names the
// LCRegisterPlayerError code the handler replies with, and whether the
// connection survives:
//
//   EmptyID                EMPTY_ID               dropped
//   ShortID                SMALL_ID_LENGTH        dropped
//   InvalidID              ETC_ERROR              dropped
//   EmptyPassword          EMPTY_PASSWORD         dropped
//   ShortPassword          SMALL_PASSWORD_LENGTH  dropped
//   EmptyName              EMPTY_NAME             dropped
//   EmptySSN               EMPTY_SSN              dropped
//   InvalidProfileField    ETC_ERROR              dropped
//   PasswordHashingFailed  ETC_ERROR              dropped
//   AlreadyRegistered      ALREADY_REGISTER_ID    kept, one failure counted
//
// CLRegisterPlayer::read already refuses an id shorter than four
// characters, a password shorter than six, and an empty id, password, name
// or registration number, so the seven length and emptiness values above
// are not producible by a client that speaks the protocol. They are
// answered all the same, in the order listed.
enum class RegisterPlayerRejection {
    EmptyID,
    ShortID,
    // The id would not survive being interpolated into SQL text.
    InvalidID,
    EmptyPassword,
    ShortPassword,
    EmptyName,
    EmptySSN,
    // One of the profile fields would not survive being interpolated into
    // SQL text, or the sex does not name a supported value.
    InvalidProfileField,
    // The password could not be hashed. A server fault, not the player's,
    // but it is answered like the refusals above.
    PasswordHashingFailed,
    // The id is taken.
    AlreadyRegistered
};

// A refusal, with what the handler's log line needs.
struct RegisterPlayerRefusal {
    RegisterPlayerRejection reason = RegisterPlayerRejection::EmptyID;

    // Only PasswordHashingFailed carries one: what the hashing library
    // reported. It never holds the password or any part of a hash.
    std::string detail;
};

// A registration request: the CLRegisterPlayer packet's fields.
//
// password is the plaintext the client sent. It is read by the two length
// checks and by the hashing, and reaches neither the accepted account nor
// any log line.
struct RegisterPlayerRequest {
    std::string playerID;
    std::string password;
    std::string name;
    // Keep unvalidated values representable until the decision checks them.
    int sex = FEMALE;
    std::string ssn;
    std::string telephone;
    std::string cellular;
    std::string zipCode;
    std::string address;
    int nation = 0;
    std::string email;
    std::string homepage;
    std::string profile;
    bool publicProfile = false;
};

struct RegisterPlayerActions {
    // Return an encoded password hash. Failure details must contain no secrets.
    std::function<std::string(const std::string&)> hashPassword;
};

// Decide whether an account may be registered, and with which row.
//
// The repository is passed in because the id probe is a database read; the
// INSERT, the log-on mark and the read-back of the new row stay with the
// caller, so this function is a pure decision over whatever the repository
// answers and needs no database in a test.
//
// The accepted LoginNewAccount carries the argon2id hash in its password
// field, never the plaintext. The supplied hasher runs after validation and
// before the id probe. Standard exceptions from hashing become a hashing
// refusal; other failures propagate. Callbacks must not mutate the request.
//
// Repository failures propagate to the caller, which owns the reply policy.
[[nodiscard]] Outcome<LoginNewAccount, RegisterPlayerRefusal>
decideRegisterPlayer(const RegisterPlayerRequest& request, LoginAccountRepository& repository,
                     const RegisterPlayerActions& actions);

#endif
