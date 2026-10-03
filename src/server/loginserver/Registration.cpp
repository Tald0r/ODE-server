//////////////////////////////////////////////////////////////////////////////
// Filename    : Registration.cpp
// Description : the account-registration decision behind
//               CLRegisterPlayerHandler.
//////////////////////////////////////////////////////////////////////////////

#include "Registration.h"

#include <exception>
#include <utility>

namespace {

// The shortest id and password an account may be registered with.
const std::string::size_type kMinIDLength = 4;
const std::string::size_type kMinPasswordLength = 6;

// Every string in the registration packet except the password (only its
// argon2 hash reaches SQL) is interpolated into SQL text verbatim, so
// anything that could break out of a quoted literal is refused.
bool containsSqlMetaCharacter(const std::string& s) {
    return s.find_first_of("'\\\";") != std::string::npos;
}

RegisterPlayerRefusal refusal(RegisterPlayerRejection reason) {
    RegisterPlayerRefusal rejection;
    rejection.reason = reason;
    return rejection;
}

} // namespace

Outcome<LoginNewAccount, RegisterPlayerRefusal> decideRegisterPlayer(const RegisterPlayerRequest& request,
                                                                     LoginAccountRepository& repository,
                                                                     const RegisterPlayerActions& actions) {
    typedef Outcome<LoginNewAccount, RegisterPlayerRefusal> Result;

    if (request.playerID.empty())
        return Result::Rejected(refusal(RegisterPlayerRejection::EmptyID));

    if (request.playerID.size() < kMinIDLength)
        return Result::Rejected(refusal(RegisterPlayerRejection::ShortID));

    if (containsSqlMetaCharacter(request.playerID))
        return Result::Rejected(refusal(RegisterPlayerRejection::InvalidID));

    if (request.password.empty())
        return Result::Rejected(refusal(RegisterPlayerRejection::EmptyPassword));

    if (request.password.size() < kMinPasswordLength)
        return Result::Rejected(refusal(RegisterPlayerRejection::ShortPassword));

    if (request.name.empty())
        return Result::Rejected(refusal(RegisterPlayerRejection::EmptyName));

    if (request.ssn.empty())
        return Result::Rejected(refusal(RegisterPlayerRejection::EmptySSN));

    if ((request.sex != FEMALE && request.sex != MALE) || containsSqlMetaCharacter(request.name) ||
        containsSqlMetaCharacter(request.ssn) || containsSqlMetaCharacter(request.telephone) ||
        containsSqlMetaCharacter(request.cellular) || containsSqlMetaCharacter(request.zipCode) ||
        containsSqlMetaCharacter(request.address) || containsSqlMetaCharacter(request.email) ||
        containsSqlMetaCharacter(request.homepage) || containsSqlMetaCharacter(request.profile)) {
        return Result::Rejected(refusal(RegisterPlayerRejection::InvalidProfileField));
    }

    // Only the hash is stored; the login verifies against it in C++.
    LoginNewAccount account;
    try {
        account.password = actions.hashPassword(request.password);
    } catch (const std::exception& e) {
        RegisterPlayerRefusal rejection = refusal(RegisterPlayerRejection::PasswordHashingFailed);
        rejection.detail = e.what();
        return Result::Rejected(rejection);
    }

    if (repository.accountExists(request.playerID))
        return Result::Rejected(refusal(RegisterPlayerRejection::AlreadyRegistered));

    account.playerID = request.playerID;
    account.name = request.name;
    account.sex = Sex2String[request.sex];
    account.ssn = request.ssn;
    account.telephone = request.telephone;
    account.cellular = request.cellular;
    account.zipCode = request.zipCode;
    account.address = request.address;
    account.nation = request.nation;
    account.email = request.email;
    account.homepage = request.homepage;
    account.profile = request.profile;
    account.pub = request.publicProfile ? "PUBLIC" : "PRIVATE";

    return Result::Ok(std::move(account));
}
