//////////////////////////////////////////////////////////////////////////////
// Filename    : LoginDecision.cpp
// Description : the login decision behind CLLoginHandler.
//////////////////////////////////////////////////////////////////////////////

#include "LoginDecision.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>

#include "PasswordHash.h"
#include "StringStream.h"
#include "Utility.h"

namespace {

// A login may fail this many times on one connection before it is dropped.
const unsigned int kMaxFailure = 3;

// A web login's key is good for five minutes.
const int kWebLoginKeyLifetimeSecs = 300;

// A hash of a throwaway string under the current parameters. It is verified
// in place of a missing row so that an unknown account costs the same time
// as a wrong password and the two cannot be told apart by reply latency.
constexpr const char* kDecoyHash =
    "$argon2id$v=19$m=65536,t=3,p=1$lCuWLGlkLo6TwTznmqikVg$73WRQ/vIrr1aYRbsplMBD4KT9aNOiTeTJr3EjQRIVS8";

LoginRejection refusal(LoginRejectReason reason, bool beginSession = false) {
    LoginRejection rejection;
    rejection.reason = reason;
    rejection.beginSession = beginSession;
    return rejection;
}

// A refusal at one of the two sites that consult the failure counter.
LoginRejection countedRefusal(LoginRejectReason reason, unsigned int failureCount) {
    LoginRejection rejection = refusal(reason, true);
    rejection.touchesFailureCount = true;
    rejection.failureCount = failureCount;
    rejection.disconnect = failureCount > kMaxFailure;
    return rejection;
}

// The adult flag the registered number implies. A login that carries no
// password of its own, other than a web login, is not classified at all.
bool decideAdult(const LoginRequest& request, const std::string& ssn, const VSDateTime& now) {
    if (request.freePass && !request.webLogin)
        return false;

    // Korea.
    if (strstr(ssn.c_str(), "-") != NULL)
        return isAdultByBirthday(ssn.substr(0, 6), now);

    if (ssn.size() == 15)
        return isAdultByBirthday(ssn.substr(6, 12), now);
    if (ssn.size() == 18)
        return isAdultByBirthday(ssn.substr(8, 14), now);
    return false;
}

} // namespace

PasswordCheck checkStoredPassword(const std::string& playerID, const std::string& password,
                                  LoginAccountRepository& repository) {
    PasswordCheck check;

    std::string stored;
    if (!repository.loadPasswordHash(playerID, stored)) {
        de::password::verify(kDecoyHash, password);
        return check;
    }

    const de::password::Verify verdict = de::password::verify(stored, password);
    if (verdict == de::password::Verify::Rejected)
        return check;

    check.accepted = true;

    if (verdict == de::password::Verify::AcceptedRehash) {
        try {
            check.hash = de::password::hash(password);
            check.rehash = true;
        } catch (const std::exception& e) {
            // The password was already accepted against the stored value,
            // so the login stands and the next one retries the rewrite.
            filelog("loginfail.txt", "Password rehash failed, PlayerID : %s : %s", playerID.c_str(), e.what());
        }
    }

    return check;
}

bool isBlockedIP(const std::string& ip, LoginAccountRepository& repository) {
    size_t i = ip.find_first_of('.', 0);
    size_t j = ip.find_first_of('.', i + 1);
    size_t k = ip.find_first_of('.', j + 1);

    /*
     * ip = 61.78.53.228
     * classA = 61
     * classB = 61.78
     * classC = 61.78.53
     */
    std::string classA = ip.substr(0, i);
    std::string classB = ip.substr(0, j);
    std::string classC = ip.substr(0, k);

    std::vector<LoginIPBlockRow> blocks = repository.loadIPBlocks(classA, classB, classC);

    for (size_t n = 0; n < blocks.size(); n++) {
        int ipClass = blocks[n].ipClass;
        int first = blocks[n].first;
        int last = blocks[n].last;
        int index;

        switch (ipClass) {
        case 0:
            index = atoi(ip.substr(k + 1, ip.size() - k - 1).c_str());
            break;
        case 1:
            index = atoi(ip.substr(i + 1, j - i - 1).c_str());
            break;
        case 2:
            index = atoi(ip.substr(j + 1, k - j - 1).c_str());
            break;
        default:
            index = -1;
            break;
        }

        if (index < 0)
            return true;

        if (index >= first && index <= last)
            return true;
    }

    return false;
}

Outcome<void, LoginRejection> decideWebLoginKey(const std::string& playerID, const std::string& key,
                                                LoginAccountRepository& repository, const WebLoginKeyActions& actions) {
    typedef Outcome<void, LoginRejection> Result;

    std::string storedKey;
    std::string createTime;
    std::string nowText;

    if (!repository.loadWebLoginKey(playerID, storedKey, createTime, nowText))
        return Result::Rejected(refusal(LoginRejectReason::WebLoginKeyNotFound));

    if (storedKey != key) {
        try {
            if (actions.mismatch)
                actions.mismatch(playerID);
        } catch (...) {
        }
        return Result::Rejected(refusal(LoginRejectReason::WebLoginKeyMismatch));
    }

    VSDateTime vsCreate(createTime);
    VSDateTime vsNow(nowText);

    if (vsCreate.secsTo(vsNow) > kWebLoginKeyLifetimeSecs)
        return Result::Rejected(refusal(LoginRejectReason::WebLoginKeyExpired));

    return Result::Ok();
}

bool isAdultByBirthday(const std::string& birthday, const VSDateTime& now) {
    StringStream adultSSN;

    // The year is the tm_year form (years since 1900), which is what the
    // number is compared against.
    adultSSN << now.date().year() - 1900 - 18;
    if (now.date().month() < 10)
        adultSSN << "0";
    adultSSN << now.date().month();
    if (now.date().day() < 10)
        adultSSN << "0";
    adultSSN << now.date().day();

    return atoi(birthday.c_str()) <= atoi(adultSSN.toString().c_str());
}

Outcome<LoginAccepted, LoginRejection> decideLogin(const LoginRequest& request, LoginAccountRepository& repository,
                                                   LoginSession& session, const VSDateTime& now) {
    typedef Outcome<LoginAccepted, LoginRejection> Result;

    // Only the id reaches SQL text; the password is verified in C++.
    if (request.playerID.find_first_of("'\\", 0) < request.playerID.size())
        return Result::Rejected(refusal(LoginRejectReason::MalformedID));

    // The account row, in the projection the login kind reads; a web login
    // and a NetMarble free pass skip the password.
    LoginAccountRow account;
    bool found = false;

    if (request.webLogin) {
        found = repository.loadAccountForWebLogin(request.playerID, account);
    } else if (request.freePass) {
        found = repository.loadAccountForFreePass(request.playerID, account);
    } else {
        found = repository.loadAccount(request.playerID, account);
    }

    // An unknown id and a wrong password get the same answer.
    if ((!found || !request.passwordAccepted) && !request.freePass)
        return Result::Rejected(countedRefusal(LoginRejectReason::UnknownAccountOrPassword, request.failureCount));

    // A NetMarble account that the free-pass check did not create. The
    // handler answers this one twice, because the original fell through to
    // the access check below with an empty Access column.
    if (request.freePass && !request.webLogin && !found)
        return Result::Rejected(refusal(LoginRejectReason::FreePassAccountMissing, true));

    LoginAccepted accepted;
    accepted.playerID = account.playerID;
    accepted.currentServerGroupID = account.currentServerGroupID;

    if (request.webLogin) {
        accepted.ssn = account.ssn;
        accepted.zipCode = "000-000";
    } else if (request.freePass) {
        // The free-pass projection selects neither column.
        accepted.zipCode = "000-000";
    } else {
        accepted.ssn = account.ssn;
        accepted.zipCode = account.zipCode;
    }

    session.setServerGroupID(accepted.currentServerGroupID);

    if (account.access != "ALLOW")
        return Result::Rejected(refusal(LoginRejectReason::AccessNotAllowed, true));

    session.setPayPlayValue(account.payType, account.payPlayDate, account.payPlayHours, account.payPlayFlag,
                            account.familyPayPlayDate);

    // An account already in a game cannot log in again. LOGON refuses
    // outright; GAME is taken over only from the address that holds it.
    bool sameIP = false;

    if (account.logOn == "LOGON" || account.logOn == "GAME") {
        if (account.logOn == "LOGON" || request.connectIP != account.loginIP)
            return Result::Rejected(countedRefusal(LoginRejectReason::AlreadyConnected, request.failureCount));

        sameIP = true;
    }

    if (sameIP) {
        // The reply waits on the game server's answer, so the values the
        // OK packet needs are settled now and kept on the session.
        accepted.adult = decideAdult(request, accepted.ssn, now);
        accepted.next = LoginNextStep::KickCharacter;
        return Result::Ok(std::move(accepted));
    }

    if (account.logOn != "LOGOFF" && account.logOn != "LOGON") {
        accepted.next = LoginNextStep::NoReply;
        return Result::Ok(std::move(accepted));
    }

    // Only a LOGOFF row flips to LOGON; a row that did not change is held
    // by another session, possibly on another login server.
    if (!session.accountOwnership().acquire(accepted.playerID, [&] {
            return repository.markLoggedOn(request.connectIP, request.loginServerID, accepted.playerID);
        }))
        return Result::Rejected(refusal(LoginRejectReason::AlreadyLoggedOnElsewhere, true));

    accepted.adult = decideAdult(request, accepted.ssn, now);
    if (request.useNetMarbleAdultFlag)
        accepted.adult = request.netMarbleAdultFlag;

    if (account.payType == 0) {
        accepted.lastDays = 0xfffe;
    } else {
        const int lastDays = now.daysTo(session.payPlayAvailableDateTime());
        const int lastSecs = now.secsTo(session.payPlayAvailableDateTime());
        const int familyLastDays = now.daysTo(session.familyPayPlayAvailableDateTime());
        const int familyLastSecs = now.secsTo(session.familyPayPlayAvailableDateTime());

        if (lastSecs < 0 && familyLastSecs < 0) {
            accepted.lastDays = 0xfffe;
        } else if (lastSecs < familyLastSecs) {
            accepted.family = true;
            accepted.lastDays = familyLastDays;

            std::cout << "Family billing plan" << std::endl;
        } else {
            accepted.family = false;
            accepted.lastDays = lastDays;
            std::cout << "Premium billing plan" << std::endl;
        }
    }

    if (accepted.lastDays > 1000)
        filelog("PayPlayDateLog.txt", "UserID : %s , LastDays : %ld", accepted.playerID.c_str(), accepted.lastDays);

    // The comeback event: an account that has not yet received its premium
    // week gets seven days of pay-play and is told so.
    if (repository.hasUnclaimedPremiumEvent(accepted.playerID)) {
        accepted.grantPremiumWeek = true;
        accepted.lastDays = 0xfffd;
    }

    accepted.next = LoginNextStep::LoginOK;
    return Result::Ok(std::move(accepted));
}
